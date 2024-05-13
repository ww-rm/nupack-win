#include <nupack/model/Model.h>
#include <nupack/model/ParameterSet.h>
#include <nupack/common/Error.h>
#include <nupack/common/Runtime.h>
#include <nupack/state/State.h>
#include <nupack/reflect/Serialize.h>

#include <fstream>
#include <boost/algorithm/string.hpp>

namespace nupack {

Base standardize_base(Base b, Alphabet const &a_orig, Alphabet const &a_new){
    auto const &base_name = a_orig.get().base_names[b.value];
    if (auto i = find_index(a_new.get().base_names, base_name); i != len(a_new.get().base_names))
        return Base::from_index(i);
    NUPACK_ERROR("Base not found in output alphabet", base_name, int(+b), a_new.get().base_names);
}

/******************************************************************************************/

ParameterFile::ParameterFile(string p) : path(p) {
    if (path == "RNA" || path == "rna") path = "rna06";
    if (path == "DNA" || path == "dna") path = "dna04";
    if (path.size() < 5 || path.substr(path.size() - 5) != ".json") path += ".json";
}

/******************************************************************************************/

json ParameterFile::open() const {
    string name = path;

    if (!path_exists(name)) {
        vec<string> defaults;
        boost::split(defaults, DefaultParametersPath, [](char c) {return c == ';';}, boost::token_compress_on);
        for (auto const &d : defaults) {
            name = path_join(d, path);
            if (path_exists(name)) break;
        }
    }

    if (!path_exists(name)) {
        auto s = get_env("NUPACKHOME");
        if (!s.empty()) name = path_join(path_join(s, "parameters"), path);
    }

    std::ifstream file(name);
    if (!file.good()) {
        vec<string> directories = {".", DefaultParametersPath, "$NUPACKHOME/parameters"};
        NUPACK_ERROR("failed to open parameter file ", path, directories);
    }
    json j;
    file >> j;
    return j;
}

/******************************************************************************************/

void ParameterIndex::set_length(std::uint32_t n) noexcept {
    alphabet_length = n;

    std::uint32_t start = 100;
    stack.set_length(start, n);
    dangle5.set_length(start, n);
    dangle3.set_length(start, n);
    terminal_penalty.set_length(start, n);
    coaxial_stack.set_length(start, n);
    interior_mismatch.set_length(start, n);
    interior_mismatch_1.set_length(start, n);
    terminal_mismatch.set_length(start, n);
    hairpin_mismatch.set_length(start, n);
    hairpin_tetra.set_length(start, n);
    hairpin_tri.set_length(start, n);
    interior_1_1.set_length(start, n);
    interior_1_2.set_length(start, n);
    interior_2_2.set_length(start, n);
}

std::uint32_t ParameterIndex::calculate_size(std::uint32_t n) const noexcept {
    if (is_condensed) {
        return hairpin_tri.begin + hairpin_tri.calculate_size(n);
    } else {
        return interior_2_2.begin + interior_2_2.calculate_size(n);
    }
}

/******************************************************************************************/

ParameterSet<real> load_parameter_set(ParameterInfo i) {
    ParameterSet<real> p;
    p.info = std::move(i);

    json const j = p.info.file.open();
    j.at("alphabet").get_to(p.alphabet);
    j.at("material").get_to(p.material);
    p.pairing = load_pairing(p.alphabet, j.at("pairs"));
    p.set_length(p.alphabet.length());

    auto data = load_parameter_data(p.alphabet, p, j.at("dG"));

    real const T = p.info.temperature;
    if (T != DefaultTemperature) {
        real kg = T / DefaultTemperature, kh = 1 - kg;
        zip(data, load_parameter_data(p.alphabet, p, j.at("dH")), [kg, kh](real &g, real const &h) {
            g = kg * g + kh * h;
        });
    }

    data.begin()[p.join_penalty().index(p.alphabet_length)] -= std::log(water_molarity(T)) * Kb * T;

    // fill(data, 0);

    auto fix = [&](auto key) {for (auto &x : data.span(key, p.alphabet.length())) x += p.info.loop_bias;};

    fix(p.stack());
    fix(p.bulge_size());
    fix(p.interior_size());
    fix(p.hairpin_size());
    fix(p.join_penalty());
    fix(p.multi_init());
    if (!p.is_condensed) {
        fix(p.interior_1_1());
        fix(p.interior_1_2());
        fix(p.interior_2_2());
    }

    p.data = std::move(data);
    p.conversion_cache = ParameterArray<float>(p.data);
    return p;
}

/******************************************************************************************/

std::array<char const *, 7> EnsembleNames = {"nostacking", "stacking", "dangle", "coaxial", "min", "all", "none"};

Ensemble as_ensemble(string_view s) {
    if (auto it = find(EnsembleNames, s); it != EnsembleNames.end())
        return Ensemble(it - EnsembleNames.begin());
    NUPACK_ERROR("invalid ensemble type", s);
}

/******************************************************************************************/

int find_loop_structure_nick(SequenceList const &c, PairList const &p) {
    NUPACK_REQUIRE(nt(c), ==, len(p));
    int nick = -1;
    auto const n_pairs = p.n_pairs();

    for (auto const &s : c) NUPACK_REQUIRE(len(s), >, 0, "Strands should have nonzero length");
    auto const nnt = nt(c);
    // find a nick ...
    if (nnt == 2 && n_strands(c) == 2) {
        nick = 0; // horrific edge case.
    } else {
        izip(prefixes(false, indirect_view(c, len)), [&](auto i, auto n) { // n is first index of the strand after i
            if (p[n-1] != n % nnt)
                nick = (i+1) % n_strands(c);
        });
        if (n_pairs + int(nick != -1) != n_strands(c)) {
            NUPACK_ERROR("Incorrect number of base pairs for a loop secondary structure", n_pairs, n_strands(c), nick);
        }
    }

    return nick;
}

/******************************************************************************************/

// Insert null bases if there is a nick and they don't exist
SequenceList complex_to_loop(Complex const &c, int nick) {
    NUPACK_ASSERT(nick >= -2 && nick < int(len(c)), "invalid nick", c, nick);
    return imap(c, [&](int i, auto const &s) {
        NUPACK_ASSERT(!s.empty(), "Loop contains an empty sequence");
        bool prepend = (nick == i) && front(s) != Base::null();
        bool append = (nick == (i+1 == len(c) ? 0 : i+1)) && back(s) != Base::null();
        return Sequence(len(s) + prepend + append, [&](Base *p, auto length) {
            NUPACK_REQUIRE(length, >=, 2, "Loop sequence does not contain enough nucleotides", c, nick);
            if (prepend) *(p++) = Base::null();
            p = std::copy(s.begin(), s.end(), p);
            if (append) *(p++) = Base::null();
        }, s.id);
    });
}

/******************************************************************************************/

struct ParameterInput {
    std::array<Base, CharCapacity> char_to_base;
    std::uint32_t n;

    ParameterInput(Alphabet const &a) : char_to_base(a.data->bases), n(a.length()) {}

    Base operator()(char c) const {
        auto o = char_to_base[c];
        if (BOOST_LIKELY(+o < n)) return o;
        NUPACK_ERROR("Invalid character in parameter file", c, int(c));
    }

    // String of bases to an array of indices (known length)
    template <std::size_t N, class S>
    std::array<BaseIndex, N> to_array(S const &key) const {
        std::array<BaseIndex, N> out;
        NUPACK_REQUIRE(N, ==, key.size(), "Incorrect number of nucleotides");
        zip(out, key, [&](auto &o, auto c) {o = +(*this)(c);});
        return out;
    }

    template <class V, class I>
    void load_array(V &&v, I const &i, json const &x) const {
        for (auto const &[key, value] : x.items()) {
            // if (info) print(key, value, i.begin, n, vmap<vec<int>>(to_array<I::ndim>(key)), array_index(i, n, to_array<I::ndim>(key)));
            value.get_to(v[array_index(i, n, to_array<I::ndim>(key))]);
            // if (info) print(v[array_index(i, n, to_array<I::ndim>(key))]);
        }
    }

    template <class P, class T>
    void simple_load(P &p, T t, json const &j) const {
        if constexpr(T::ndim == 0) {
            j.get_to(*p.span(t, n).begin());
        } else {
            auto s = p.span(t, n);
            NUPACK_REQUIRE(j.size(), ==, s.size());
            std::copy(j.begin(), j.end(), s.begin());
        }
    }
};

/******************************************************************************************/

ParameterArray<real> load_parameter_data(Alphabet const &a, ParameterIndex &i, json const &j) {
    ParameterInput input(a);

    i.is_condensed = !j.contains("interior_1_1");
    ParameterArray<real> p(i.calculate_size(a.length()));
    fill(p, 0.0);

    input.simple_load(p, i.log_loop_penalty(), j.at("log_loop_penalty"));
    input.simple_load(p, i.hairpin_size(),     j.at("hairpin_size"));
    input.simple_load(p, i.bulge_size(),       j.at("bulge_size"));
    input.simple_load(p, i.multi_init(),       j.at("multiloop_init"));
    input.simple_load(p, i.multi_pair(),       j.at("multiloop_pair"));
    input.simple_load(p, i.multi_base(),       j.at("multiloop_base"));
    input.simple_load(p, i.join_penalty(),     j.at("join_penalty"));
    input.simple_load(p, i.interior_size(),    j.at("interior_size"));
    input.simple_load(p, i.ninio(),            j.at("asymmetry_ninio"));

    input.load_array(p.begin(), i.stack(),               j.at("stack"));
    input.load_array(p.begin(), i.coaxial_stack(),       j.at("coaxial_stack"));
    input.load_array(p.begin(), i.hairpin_tri(),         j.at("hairpin_triloop"));
    input.load_array(p.begin(), i.hairpin_tetra(),       j.at("hairpin_tetraloop"));
    input.load_array(p.begin(), i.hairpin_mismatch(),    j.at("hairpin_mismatch"));
    input.load_array(p.begin(), i.interior_mismatch(),   j.at("interior_mismatch"));
    input.load_array(p.begin(), i.interior_mismatch_1(), j.at("interior_mismatch_1"));
    input.load_array(p.begin(), i.terminal_mismatch(),   j.at("terminal_mismatch"));
    input.load_array(p.begin(), i.dangle5(),             j.at("dangle_5"));
    input.load_array(p.begin(), i.dangle3(),             j.at("dangle_3"));
    input.load_array(p.begin(), i.terminal_penalty(),    j.at("terminal_penalty"));

    if (!i.is_condensed) {
        input.load_array(p.begin(), i.interior_1_1(),        j.at("interior_1_1"));
        input.load_array(p.begin(), i.interior_1_2(),        j.at("interior_1_2"));
        input.load_array(p.begin(), i.interior_2_2(),        j.at("interior_2_2"));
    }

    return p;
}

/******************************************************************************************/

struct ParameterOutput {
    std::array<char, Base::capacity+1> base_to_char;
    std::uint32_t n;

    ParameterOutput(Alphabet const &a) : base_to_char(a.data->letters), n(a.length()) {}

    char operator()(Base b) const {return base_to_char[+b];}

    template <class ...Is>
    std::string to_string(Is const ...is) const {
        std::string out;
        (out.push_back((*this)(is)), ...);
        return out;
    }

    template <class T, class V, class ...Is>
    void save_to_array(json &j, T const &t, V const &v, Is const ...is) const {
        if constexpr(sizeof...(Is) == T::ndim) {
            auto value = v[t.index(n, is...)];
            if (value != 0) j[to_string(Base::from_index(is)...)] = value;
        } else {
            for (auto i : range(BaseIndex(n))) save_to_array(j, t, v, i, is...);
        }
    }

    template <class V, class T>
    json save_array(V const &v, T const &t) {
        json out = json::object();
        save_to_array(out, t, v);
        return out;
    }

    template <class P, class T>
    json simple_save(P const &p, T const &t) const {
        if constexpr(T::ndim == 0) {
            return p.begin()[t.start()];
        } else {
            auto out = json::array();
            for (auto const &x : p.span(t, n)) out.emplace_back(x);
            return out;
        }
    }
};

/******************************************************************************************/

json save_parameter_data(Alphabet const &a, ParameterIndex const &i, ParameterArray<real> const &p) {
    json j;
    if (!p.begin()) return j;

    ParameterOutput output(a);

    j["log_loop_penalty"] =    output.simple_save(p, i.log_loop_penalty());
    j["hairpin_size"] =        output.simple_save(p, i.hairpin_size());
    j["bulge_size"] =          output.simple_save(p, i.bulge_size());
    j["multiloop_init"] =      output.simple_save(p, i.multi_init());
    j["multiloop_pair"] =      output.simple_save(p, i.multi_pair());
    j["multiloop_base"] =      output.simple_save(p, i.multi_base());
    j["join_penalty"] =        output.simple_save(p, i.join_penalty());
    j["interior_size"] =       output.simple_save(p, i.interior_size());
    j["asymmetry_ninio"] =     output.simple_save(p, i.ninio());
    j["stack"] =               output.save_array(p.begin(), i.stack());
    j["coaxial_stack"] =       output.save_array(p.begin(), i.coaxial_stack());
    j["hairpin_triloop"] =     output.save_array(p.begin(), i.hairpin_tri());
    j["hairpin_tetraloop"] =   output.save_array(p.begin(), i.hairpin_tetra());
    j["hairpin_mismatch"] =    output.save_array(p.begin(), i.hairpin_mismatch());
    j["interior_mismatch"] =   output.save_array(p.begin(), i.interior_mismatch());
    j["interior_mismatch_1"] = output.save_array(p.begin(), i.interior_mismatch_1());
    j["terminal_mismatch"] =   output.save_array(p.begin(), i.terminal_mismatch());
    j["dangle_5"] =            output.save_array(p.begin(), i.dangle5());
    j["dangle_3"] =            output.save_array(p.begin(), i.dangle3());
    j["terminal_penalty"] =    output.save_array(p.begin(), i.terminal_penalty());

    if (!i.is_condensed) {
        j["interior_1_1"] =        output.save_array(p.begin(), i.interior_1_1());
        j["interior_1_2"] =        output.save_array(p.begin(), i.interior_1_2());
        j["interior_2_2"] =        output.save_array(p.begin(), i.interior_2_2());
    }
    return j;
}

/******************************************************************************************/

}

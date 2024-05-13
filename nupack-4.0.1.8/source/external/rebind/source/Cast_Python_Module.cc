#include <rebind-python/Cast.h>
#include <numeric>

namespace rebind {

bool is_subclass(PyTypeObject *o, PyTypeObject *t) {
    int x = PyObject_IsSubclass(reinterpret_cast<PyObject *>(o), reinterpret_cast<PyObject *>(t));
    return (x < 0) ? throw python_error() : x;
}

/******************************************************************************/

Object type_args(Object const &o) {
    auto out = Object::from(PyObject_GetAttrString(o, "__args__"));
    if (out && !PyTuple_Check(out))
        throw python_error(type_error("expected __args__ to be a tuple"));
    return out;
}

Object type_args(Object const &o, Py_ssize_t n) {
    auto out = type_args(o);
    if (out) {
        Py_ssize_t const m = PyTuple_GET_SIZE(+out);
        if (m != n) throw python_error(type_error("expected __args__ to be length %zd (got %zd)", n, m));
    }
    return out;
}

Object list_cast(Variable &&ref, Object const &o, Object const &root) {
    DUMP("Cast to list ", ref.type());
    if (auto args = type_args(o, 1)) {
        DUMP("is list ", PyList_Check(o));
        auto v = ref.cast<Sequence>();
        Object vt{PyTuple_GET_ITEM(+args, 0), true};
        auto list = Object::from(PyList_New(v.size()));
        for (Py_ssize_t i = 0; i != v.size(); ++i) {
            DUMP("list index ", i);
            Object item = python_cast(std::move(v[i]), vt, root);
            if (!item) return {};
            incref(+item);
            PyList_SET_ITEM(+list, i, +item);
        }
        return list;
    } else return {};
}

Object tuple_cast(Variable &&ref, Object const &o, Object const &root) {
    DUMP("Cast to tuple ", ref.type());
    if (auto args = type_args(o)) {
        Py_ssize_t const len = PyTuple_GET_SIZE(+args);
        auto v = ref.cast<Sequence>();
        if (len == 2 && PyTuple_GET_ITEM(+args, 1) == Py_Ellipsis) {
            Object vt{PyTuple_GET_ITEM(+args, 0), true};
            auto tup = Object::from(PyTuple_New(v.size()));
            for (Py_ssize_t i = 0; i != v.size(); ++i)
                if (!set_tuple_item(tup, i, python_cast(std::move(v[i]), vt, root))) return {};
            return tup;
        } else if (len == v.size()) {
            auto tup = Object::from(PyTuple_New(len));
            for (Py_ssize_t i = 0; i != len; ++i)
                if (!set_tuple_item(tup, i, python_cast(std::move(v[i]), {PyTuple_GET_ITEM(+args, i), true}, root))) return {};
            return tup;
        }
    }
    return {};
}

Object dict_cast(Variable &&ref, Object const &o, Object const &root) {
    DUMP("Cast to dict ", ref.type());
    if (auto args = type_args(o, 2)) {
        Object key{PyTuple_GET_ITEM(+args, 0), true};
        Object val{PyTuple_GET_ITEM(+args, 1), true};

        if (+key == SubClass<PyTypeObject>{&PyUnicode_Type}) {
            if (auto v = ref.request<Dictionary>()) {
                auto out = Object::from(PyDict_New());
                for (auto &x : *v) {
                    Object k = as_object(x.first);
                    Object v = python_cast(std::move(x.second), val, root);
                    if (!k || !v || PyDict_SetItem(out, k, v)) return {};
                }
                return out;
            }
        }

        if (auto v = ref.request<Vector<std::pair<Variable, Variable>>>()) {
            auto out = Object::from(PyDict_New());
            for (auto &x : *v) {
                Object k = python_cast(std::move(x.first), key, root);
                Object v = python_cast(std::move(x.second), val, root);
                if (!k || !v || PyDict_SetItem(out, k, v)) return {};
            }
            return out;
        }
    }
    return {};
}

// Convert Variable to a class which is a subclass of rebind.Variable
Object variable_cast(Variable &&v, Object const &t) {
    PyObject *x;
    if (t) x = +t;
    else if (!v.has_value()) return {Py_None, true};
    else if (auto it = python_types.find(v.type().info()); it != python_types.end()) x = +it->second;
    else x = type_object<Variable>();

    auto o = Object::from((x == type_object<Variable>()) ?
        PyObject_CallObject(x, nullptr) : PyObject_CallMethod(x, "__new__", "O", x));

    DUMP("making variable ", v.type());
    cast_object<Variable>(o) = std::move(v);
    DUMP("made variable ", v.type());
    return o;
}

/******************************************************************************/

Object bool_cast(Variable &&ref) {
    if (auto p = ref.request<bool>()) return as_object(*p);
    return {};
}

Object int_cast(Variable &&ref) {
    if (auto p = ref.request<Integer>()) return as_object(*p);
    DUMP("bad int");
    return {};
}

Object float_cast(Variable &&ref) {
    if (auto p = ref.request<double>()) return as_object(*p);
    if (auto p = ref.request<Integer>()) return as_object(*p);
    DUMP("bad float");
    return {};
}

Object str_cast(Variable &&ref) {
    DUMP("converting ", ref.type(), " to str");
    DUMP(ref.type(), bool(ref.action()));
    if (auto p = ref.request<std::string_view>()) return as_object(std::move(*p));
    if (auto p = ref.request<std::string>()) return as_object(std::move(*p));
    if (auto p = ref.request<std::wstring_view>())
        return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
    if (auto p = ref.request<std::wstring>())
        return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
    return {};
}

Object bytes_cast(Variable &&ref) {
    if (auto p = ref.request<BinaryView>()) return as_object(std::move(*p));
    if (auto p = ref.request<Binary>()) return as_object(std::move(*p));
    return {};
}

Object type_index_cast(Variable &&ref) {
    if (auto p = ref.request<TypeIndex>()) return as_object(std::move(*p));
    else return {};
}

Object function_cast(Variable &&ref) {
    if (auto p = ref.request<Function>()) return as_object(std::move(*p));
    else return {};
}

Object memoryview_cast(Variable &&ref, Object const &root) {
    if (auto p = ref.request<ArrayView>()) {
        auto x = type_object<ArrayBuffer>();
        auto obj = Object::from(PyObject_CallObject(x, nullptr));
        cast_object<ArrayBuffer>(obj) = {std::move(*p), root};
        return Object::from(PyMemoryView_FromObject(obj));
    } else return {};
}

Object getattr(PyObject *obj, char const *name) {
    if (PyObject_HasAttrString(obj, name))
        return {PyObject_GetAttrString(obj, name), false};
    return {};
}

// condition: PyType_CheckExact(type) is false
bool is_structured_type(PyObject *type, PyObject *origin) {
    if constexpr(PythonVersion >= Version(3, 7, 0)) {
        DUMP("is_structure_type 3.7A");
        // in this case, origin may or may not be a PyTypeObject *
        return origin == +getattr(type, "__origin__");
    } else {
        // case like typing.Union: type(typing.Union[int, float] == typing.Union)
        return (+type)->ob_type == reinterpret_cast<PyTypeObject *>(origin);
    }
}

bool is_structured_type(PyObject *type, PyTypeObject *origin) {
    if constexpr(PythonVersion >= Version(3, 7, 0)) {
        DUMP("is_structure_type 3.7B");
        return reinterpret_cast<PyObject *>(origin) == +getattr(type, "__origin__");
    } else {
        // case like typing.Tuple: issubclass(typing.Tuple[int, float], tuple)
        return is_subclass(reinterpret_cast<PyTypeObject *>(type), reinterpret_cast<PyTypeObject *>(origin));
    }
}

Object union_cast(Variable &&v, Object const &t, Object const &root) {
    if (auto args = type_args(t)) {
        auto const n = PyTuple_GET_SIZE(+args);
        for (Py_ssize_t i = 0; i != n; ++i) {
            Object o = python_cast(std::move(v), {PyTuple_GET_ITEM(+args, i), true}, root);
            if (o) return o;
            else PyErr_Clear();
        }
    }
    return type_error("cannot convert value to %R from type %S", +t, +type_index_cast(v.type()));
}


// Convert C++ Variable to a requested Python type
// First explicit types are checked:
// None, object, bool, int, float, str, bytes, TypeIndex, list, tuple, dict, Variable, Function, memoryview
// Then, the output_conversions map is queried for Python function callable with the Variable
Object try_python_cast(Variable &&v, Object const &t, Object const &root) {
    DUMP("try_python_cast ", v.type());
    if (auto it = type_translations.find(t); it != type_translations.end()) {
        DUMP("type_translation found");
        return try_python_cast(std::move(v), it->second, root);
    } else if (PyType_CheckExact(+t)) {
        auto type = reinterpret_cast<PyTypeObject *>(+t);
        DUMP("is Variable ", is_subclass(type, type_object<Variable>()));
        if (+type == Py_None->ob_type || +t == Py_None)       return {Py_None, true};                        // NoneType
        else if (type == &PyBool_Type)                        return bool_cast(std::move(v));                // bool
        else if (type == &PyLong_Type)                        return int_cast(std::move(v));                 // int
        else if (type == &PyFloat_Type)                       return float_cast(std::move(v));               // float
        else if (type == &PyUnicode_Type)                     return str_cast(std::move(v));                 // str
        else if (type == &PyBytes_Type)                       return bytes_cast(std::move(v));               // bytes
        else if (type == &PyBaseObject_Type)                  return as_deduced_object(std::move(v));        // object
        else if (is_subclass(type, type_object<Variable>()))  return variable_cast(std::move(v), t);         // Variable
        else if (type == type_object<TypeIndex>())            return type_index_cast(std::move(v));          // type(TypeIndex)
        else if (type == type_object<Function>())             return function_cast(std::move(v));            // Function
        else if (is_subclass(type, &PyFunction_Type))         return function_cast(std::move(v));            // Function
        else if (type == &PyMemoryView_Type)                  return memoryview_cast(std::move(v), root);    // memory_view
    } else {
        DUMP("Not type and not in translations");
        if (auto p = cast_if<TypeIndex>(t)) { // TypeIndex
            Dispatch msg;
            if (auto var = std::move(v).request_variable(msg, *p))
                return variable_cast(std::move(var));
            std::string c1 = v.type().name(), c2 = p->name();
            return type_error("could not convert object of type %s to type %s", c1.data(), c2.data());
        }
        else if (is_structured_type(t, UnionType))     return union_cast(std::move(v), t, root);
        else if (is_structured_type(t, &PyList_Type))  return list_cast(std::move(v), t, root);       // List[T] for some T (compound type)
        else if (is_structured_type(t, &PyTuple_Type)) return tuple_cast(std::move(v), t, root);      // Tuple[Ts...] for some Ts... (compound type)
        else if (is_structured_type(t, &PyDict_Type))  return dict_cast(std::move(v), t, root);       // Dict[K, V] for some K, V (compound type)
        DUMP("Not one of the structure types");
    }

    DUMP("custom convert ", output_conversions.size());
    if (auto p = output_conversions.find(t); p != output_conversions.end()) {
        DUMP(" conversion ");
        Object o = variable_cast(std::move(v));
        if (!o) return type_error("could not cast Variable to Python object");
        DUMP("calling function");
        auto &obj = static_cast<Var &>(cast_object<Variable>(o)).ward;
        if (!obj) obj = root;
        return Object::from(PyObject_CallFunctionObjArgs(+p->second, +o, nullptr));
    }

    return nullptr;
}

Object python_cast(Variable &&v, Object const &t, Object const &root) {
    Object out = try_python_cast(std::move(v), t, root);
    if (!out) return type_error("cannot convert value to type %R from type %S", +t, +type_index_cast(v.type()));
    return out;
}

/******************************************************************************/

}

/**
 * @brief Python-related C++ source code for rebind
 * @file Python.cc
 */
#include <rebind-python/API.h>
#include <rebind/Document.h>
#include <complex>
#include <any>
#include <iostream>

namespace rebind {

/******************************************************************************/

std::pair<Object, char const *> str(PyObject *o) {
    std::pair<Object, char const *> out({PyObject_Str(o), false}, nullptr);
    if (out.first) {
#       if PY_MAJOR_VERSION > 2
            out.second = PyUnicode_AsUTF8(out.first); // PyErr_Clear
#       else
            out.second = PyString_AsString(out.first);
#       endif
    }
    return out;
}

void print(PyObject *o) {
    auto p = str(o);
    if (p.second) std::cout << p.second << std::endl;
}

// Assuming a Python exception has been raised, fetch its string and put it in
// a C++ exception type. Does not clear the Python error status.
PythonError python_error(std::nullptr_t) noexcept {
    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);
    if (!type) return PythonError("Expected Python exception to be set");
    auto p = str(value);
    PyErr_Restore(type, value, traceback);
    return PythonError(p.second ? p.second : "Python error with failed str()");
}

/******************************************************************************/

/// type_index from PyBuffer format string (excludes constness)
std::type_info const & Buffer::format(std::string_view s) {
    auto it = std::find_if(Buffer::formats.begin(), Buffer::formats.end(),
        [&](auto const &p) {return p.first == s;});
    return it == Buffer::formats.end() ? typeid(void) : *it->second;
}

std::string_view Buffer::format(std::type_info const &t) {
    auto it = std::find_if(Buffer::formats.begin(), Buffer::formats.end(),
        [&](auto const &p) {return p.second == &t;});
    return it == Buffer::formats.end() ? std::string_view() : it->first;
}

std::size_t Buffer::itemsize(std::type_info const &t) {
    auto it = std::find_if(scalars.begin(), scalars.end(),
        [&](auto const &p) {return std::get<1>(p) == t;});
    return it == scalars.end() ? 0u : std::get<2>(*it) / CHAR_BIT;
}

/******************************************************************************/

std::string_view from_unicode(PyObject *o) {
    Py_ssize_t size;
#if PY_MAJOR_VERSION > 2
    char const *c = PyUnicode_AsUTF8AndSize(o, &size);
#else
    char *c;
    if (PyString_AsStringAndSize(o, &c, &size)) throw python_error();
#endif
    if (!c) throw python_error();
    return std::string_view(static_cast<char const *>(c), size);
}

std::string_view from_bytes(PyObject *o) {
    char *c;
    Py_ssize_t size;
    PyBytes_AsStringAndSize(+o, &c, &size);
    return std::string_view(c, size);
}

/******************************************************************************/

template <class T>
bool to_arithmetic(Object const &o, Variable &v) {
    DUMP("cast arithmetic in: ", v.type());
    if (PyFloat_Check(o)) return v = static_cast<T>(PyFloat_AsDouble(+o)), true;
    if (PyLong_Check(o)) return v = static_cast<T>(PyLong_AsLongLong(+o)), true;
    if (PyBool_Check(o)) return v = static_cast<T>(+o == Py_True), true;
    if (PyNumber_Check(+o)) { // This can be hit for e.g. numpy.int64
        if (std::is_integral_v<T>) {
            if (auto i = Object::from(PyNumber_Long(+o)))
                return v = static_cast<T>(PyLong_AsLongLong(+i)), true;
        } else {
            if (auto i = Object::from(PyNumber_Float(+o)))
               return v = static_cast<T>(PyFloat_AsDouble(+i)), true;
        }
    }
    DUMP("cast arithmetic out: ", v.type());
    return false;
}

/******************************************************************************/

bool object_response(Variable &v, TypeIndex t, Object o) {
    if (Debug) {
        auto repr = Object::from(PyObject_Repr(SubClass<PyTypeObject>{(+o)->ob_type}));
        DUMP("input object reference count ", reference_count(o));
        DUMP("trying to convert object to ", t.name(), " ", from_unicode(+repr));
        DUMP(bool(cast_if<Variable>(o)));
    }

    if (auto p = cast_if<Variable>(o)) {
        DUMP("its a variable");
        Dispatch msg;
        v = p->request_variable(msg, t);
        return v.has_value();
    }

    if (t.matches<TypeIndex>()) {
        if (auto p = cast_if<TypeIndex>(o)) return v = *p, true;
        else return false;
    }

    if (t.equals<std::nullptr_t>()) {
        if (+o == Py_None) return v = nullptr, true;
    }

    if (t.matches<Function>()) {
        DUMP("requested function");
        if (+o == Py_None) v.emplace(Type<Function>());
        else if (auto p = cast_if<Function>(o)) v = *p;
        // general python function has no signature associated with it right now.
        // we could get them out via function.__annotations__ and process them into a tuple
        else v.emplace(Type<Function>())->emplace(PythonFunction({+o, true}, {Py_None, true}), {});
        return true;
    }

    if (t.equals<Sequence>()) {
        if (PyTuple_Check(o) || PyList_Check(o)) {
            DUMP("making a Sequence");
            Sequence *s = v.emplace(Type<Sequence>());
            s->reserve(PyObject_Length(o));
            map_iterable(o, [&](Object o) {s->emplace_back(std::move(o));});
            return true;
        } else return false;
    }

    if (t.equals<Real>())
        return to_arithmetic<Real>(o, v);

    if (t.equals<Integer>())
        return to_arithmetic<Integer>(o, v);

    if (t.equals<bool>()) {
        if ((+o)->ob_type == Py_None->ob_type) { // fix, doesnt work with Py_None...
            return v = false, true;
        } else return to_arithmetic<bool>(o, v);
    }

    if (t.equals<std::string_view>()) {
        if (PyUnicode_Check(+o)) return v.emplace(Type<std::string_view>(), from_unicode(+o)), true;
        if (PyBytes_Check(+o)) return v.emplace(Type<std::string_view>(), from_bytes(+o)), true;
        return false;
    }

    if (t.equals<std::string>()) {
        if (PyUnicode_Check(+o)) return v.emplace(Type<std::string>(), from_unicode(+o)), true;
        if (PyBytes_Check(+o)) return v.emplace(Type<std::string>(), from_bytes(+o)), true;
        return false;
    }

    if (t.equals<ArrayView>()) {
        if (PyObject_CheckBuffer(+o)) {
            // Read in the shape but ignore strides, suboffsets
            DUMP("cast buffer", reference_count(o));
            if (auto buff = Buffer(o, PyBUF_FULL_RO)) {
                DUMP("making data", reference_count(o));
                DUMP(Buffer::format(buff.view.format ? buff.view.format : "").name());
                DUMP("ndim", buff.view.ndim);
                DUMP((nullptr == buff.view.buf), bool(buff.view.readonly));
                for (auto i = 0; i != buff.view.ndim; ++i) DUMP(i, buff.view.shape[i], buff.view.strides[i]);
                DUMP("itemsize", buff.view.itemsize);
                ArrayLayout lay;
                lay.contents.reserve(buff.view.ndim);
                for (std::size_t i = 0; i != buff.view.ndim; ++i)
                    lay.contents.emplace_back(buff.view.shape[i], buff.view.strides[i] / buff.view.itemsize);
                DUMP("layout", lay, reference_count(o));
                DUMP("depth", lay.depth());
                ArrayData data{buff.view.buf, buff.view.format ? &Buffer::format(buff.view.format) : &typeid(void), !buff.view.readonly};
                return v.emplace(Type<ArrayView>(), std::move(data), std::move(lay)), true;
            } else throw python_error(type_error("C++: could not get buffer"));
        } else return false;
    }

    if (t.equals<std::complex<double>>()) {
        if (PyComplex_Check(+o)) return v.emplace(Type<std::complex<double>>(), PyComplex_RealAsDouble(+o), PyComplex_ImagAsDouble(+o)), true;
        return false;
    }

    DUMP("requested ", v.type(), t);
    return false;
}

/******************************************************************************/

std::string get_type_name(TypeIndex idx) noexcept {
    std::string out;
    auto it = type_names.find(idx);
    if (it == type_names.end() || it->second.empty()) out = idx.name();
    else out = it->second;
    out += QualifierSuffixes[static_cast<unsigned char>(idx.qualifier())];
    return out;
}

/******************************************************************************/

std::string wrong_type_message(WrongType const &e, std::string_view prefix) {
    std::ostringstream os;
    os << prefix << e.what() << " (#" << e.index << ", ";
    if (!e.source.empty())
        os << e.source << " \u2192 " << get_type_name(e.dest) << ", ";
    if (!e.indices.empty()) {
        auto it = e.indices.begin();
        os << "scopes=[" << *it;
        while (++it != e.indices.end()) os << ", " << *it;
        os << "], ";
    }
    if (e.expected != -1)
        os << "expected=" << e.expected << " received=" << e.received << ", ";
    std::string s = os.str();
    s.pop_back();
    s.back() = ')';
    return s;
}

/******************************************************************************/

Variable variable_reference_from_object(Object o) {
    if (auto p = cast_if<Function>(o)) return {Type<Function const &>(), *p};
    else if (auto p = cast_if<std::type_index>(o)) return {Type<std::type_index>(), *p};
    else if (auto p = cast_if<Variable>(o)) {
        DUMP("variable from object ", p, " ", p->data());
        DUMP("variable qualifier=", p->qualifier(), ", reference qualifier=", p->reference().qualifier());
        return p->reference();
    }
    else return std::move(o);
}

/******************************************************************************/

// Store the objects in args in pack
void args_from_python(Sequence &v, Object const &args) {
    v.reserve(v.size() + PyObject_Length(+args));
    map_iterable(args, [&v](Object o) {v.emplace_back(variable_reference_from_object(std::move(o)));});
}

/******************************************************************************/

}

/**
 * @brief Python-related C++ source code for rebind
 * @file Python.cc
 */
#include <rebind-python/Cast.h>
#include <rebind-python/API.h>
#include <rebind/Document.h>
#include <any>
#include <iostream>
#include <numeric>

#ifndef REBIND_MODULE
#   define REBIND_MODULE librebind
#endif

#define REBIND_CAT_IMPL(s1, s2) s1##s2
#define REBIND_CAT(s1, s2) REBIND_CAT_IMPL(s1, s2)

#define REBIND_STRING_IMPL(x) #x
#define REBIND_STRING(x) REBIND_STRING_IMPL(x)

#include "Var.cc"
#include "Function.cc"

namespace rebind {

// template <class F>
// constexpr PyMethodDef method(char const *name, F fun, int type, char const *doc) noexcept {
//     if constexpr (std::is_convertible_v<F, PyCFunction>)
//         return {name, static_cast<PyCFunction>(fun), type, doc};
//     else return {name, reinterpret_cast<PyCFunction>(fun), type, doc};
// }

/******************************************************************************/

int array_data_buffer(PyObject *self, Py_buffer *view, int flags) noexcept {
    auto p = cast_if<ArrayBuffer>(self);
    if (!p) return 1;
    view->buf = p->data;

    view->itemsize = Buffer::itemsize(*p->type);
    view->len = p->n_elem;
    view->readonly = !p->mutate;
    view->format = const_cast<char *>(Buffer::format(*p->type).data());
    view->ndim = p->shape_stride.size() / 2;
    view->shape = p->shape_stride.data();
    view->strides = p->shape_stride.data() + view->ndim;
    view->suboffsets = nullptr;
    view->obj = self;
    ++p->exports;
    DUMP("allocating new array buffer", bool(p->base));
    incref(view->obj);
    return 0;
}

void array_data_release(PyObject *self, Py_buffer *view) noexcept {
    // auto &p  = cast_object<ArrayBuffer>(self)
    // --p.exports;
    // if (p.exports)
    DUMP("releasing array buffer");
}

PyBufferProcs buffer_procs{array_data_buffer, array_data_release};

template <>
PyTypeObject Holder<ArrayBuffer>::type = []{
    auto o = type_definition<ArrayBuffer>("rebind.ArrayBuffer", "C++ ArrayBuffer object");
    o.tp_as_buffer = &buffer_procs;
    return o;
}();

/******************************************************************************/

PyObject *type_index_new(PyTypeObject *subtype, PyObject *, PyObject *) noexcept {
    PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    if (o) new (&cast_object<TypeIndex>(o)) TypeIndex(typeid(void)); // noexcept
    return o;
}

long long type_index_hash(PyObject *o) noexcept {
    return static_cast<long long>(cast_object<TypeIndex>(o).hash_code());
}

PyObject *type_index_repr(PyObject *o) noexcept {
    TypeIndex const *p = cast_if<TypeIndex>(o);
    if (p) return PyUnicode_FromFormat("TypeIndex('%s')", get_type_name(*p).data());
    return type_error("Expected instance of rebind.TypeIndex");
}

PyObject *type_index_str(PyObject *o) noexcept {
    TypeIndex const *p = cast_if<TypeIndex>(o);
    if (p) return PyUnicode_FromString(get_type_name(*p).data());
    return type_error("Expected instance of rebind.TypeIndex");
}

PyObject *type_index_compare(PyObject *self, PyObject *other, int op) {
    return raw_object([=]() -> Object {
        return {compare(op, cast_object<TypeIndex>(self), cast_object<TypeIndex>(other)) ? Py_True : Py_False, true};
    });
}

template <>
PyTypeObject Holder<TypeIndex>::type = []{
    auto o = type_definition<TypeIndex>("rebind.TypeIndex", "C++ type_index object");
    o.tp_repr = type_index_repr;
    o.tp_hash = type_index_hash;
    o.tp_str = type_index_str;
    o.tp_richcompare = type_index_compare;
    return o;
}();

/******************************************************************************/

bool attach_type(Object const &m, char const *name, PyTypeObject *t) noexcept {
    if (PyType_Ready(t) < 0) return false;
    incref(reinterpret_cast<PyObject *>(t));
    return PyDict_SetItemString(+m, name, reinterpret_cast<PyObject *>(t)) >= 0;
}

bool attach(Object const &m, char const *name, Object o) noexcept {
    return o && PyDict_SetItemString(m, name, o) >= 0;
}

/******************************************************************************/

Object initialize(Document const &doc) {
    initialize_global_objects();

    auto m = Object::from(PyDict_New());
    for (auto const &p : doc.types)
        if (p.second) type_names.emplace(p.first, p.first.name());//p.second->first);

    if (PyType_Ready(type_object<ArrayBuffer>()) < 0) return {};
    incref(type_object<ArrayBuffer>());

    bool ok = attach_type(m, "Variable", type_object<Variable>())
        && attach_type(m, "Function", type_object<Function>())
        && attach_type(m, "TypeIndex", type_object<TypeIndex>())
        && attach_type(m, "DelegatingFunction", type_object<DelegatingFunction>())
        && attach_type(m, "DelegatingMethod", type_object<DelegatingMethod>())
        && attach_type(m, "Method", type_object<Method>())
            // Tuple[Tuple[int, TypeIndex, int], ...]
        && attach(m, "scalars", map_as_tuple(scalars, [](auto const &x) {
            return args_as_tuple(as_object(static_cast<Integer>(std::get<0>(x))),
                                 as_object(static_cast<TypeIndex>(std::get<1>(x))),
                                 as_object(static_cast<Integer>(std::get<2>(x))));
        }))
            // Tuple[Tuple[TypeIndex, Tuple[Tuple[str, function], ...]], ...]
        && attach(m, "contents", map_as_tuple(doc.contents, [](auto const &x) {
            Object o;
            if (auto p = x.second.template target<Function const &>()) o = as_object(*p);
            else if (auto p = x.second.template target<TypeIndex const &>()) o = as_object(*p);
            else if (auto p = x.second.template target<TypeData const &>()) o = args_as_tuple(
                map_as_tuple(p->methods, [](auto const &x) {return args_as_tuple(as_object(x.first), as_object(x.second));}),
                map_as_tuple(p->data, [](auto const &x) {return args_as_tuple(as_object(x.first), variable_cast(Variable(x.second)));})
            );
            else o = variable_cast(Variable(x.second));
            return args_as_tuple(as_object(x.first), std::move(o));
        }))
        && attach(m, "set_output_conversion", as_object(Function::of([](Object t, Object o) {
            output_conversions.insert_or_assign(std::move(t), std::move(o));
        })))
        && attach(m, "set_input_conversion", as_object(Function::of([](Object t, Object o) {
            input_conversions.insert_or_assign(std::move(t), std::move(o));
        })))
        && attach(m, "set_translation", as_object(Function::of([](Object t, Object o) {
            type_translations.insert_or_assign(std::move(t), std::move(o));
        })))
        && attach(m, "clear_global_objects", as_object(Function::of(&clear_global_objects)))
        && attach(m, "set_debug", as_object(Function::of([](bool b) {return std::exchange(Debug, b);})))
        && attach(m, "debug", as_object(Function::of([] {return Debug;})))
        && attach(m, "set_type_error", as_object(Function::of([](Object o) {TypeError = std::move(o);})))
        && attach(m, "set_type", as_object(Function::of([](TypeIndex idx, Object o) {
            DUMP("set_type in");
            python_types.emplace(idx.info(), std::move(o));
            DUMP("set_type out");
        })))
        && attach(m, "set_type_names", as_object(Function::of([](Zip<TypeIndex, std::string_view> v) {
            for (auto const &p : v) type_names.insert_or_assign(p.first, p.second);
        })));
    return ok ? m : Object();
}

void init(Document &doc);

}

extern "C" {

#if PY_MAJOR_VERSION > 2

#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wmissing-field-initializers"
    PyTypeObject o{PyVarObject_HEAD_INIT(NULL, 0)};
    static struct PyModuleDef rebind_definition = {
        PyModuleDef_HEAD_INIT,
        REBIND_STRING(REBIND_MODULE),
        "A Python module to run C++ unit tests",
        -1,
    };
#   pragma clang diagnostic pop

    PyObject* REBIND_CAT(PyInit_, REBIND_MODULE)(void) {
        Py_Initialize();
        return rebind::raw_object([&]() -> rebind::Object {
            rebind::Object mod {PyModule_Create(&rebind_definition), true};
            if (!mod) return {};
            rebind::init(rebind::document());
            rebind::Object dict = initialize(rebind::document());
            if (!dict) return {};
            rebind::incref(+dict);
            if (PyModule_AddObject(+mod, "document", +dict) < 0) return {};
            return mod;
        });
    }
#else
    void REBIND_CAT(init, REBIND_MODULE)(void) {

    }
#endif
}

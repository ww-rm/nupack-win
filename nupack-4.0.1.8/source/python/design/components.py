from typing import Tuple, List, Dict, Union, Callable
import datetime, pathlib

# from .. import model # import Conditions, ParameterFile
# from .. import core
from .. import constants, utility
from ..rebind import forward

@forward
class Timer:
    """A timer for design"""
    def start(self, _fun_=None):
        """start the timer"""
        _fun_(self)

    def elapsed(self) -> float:
        """time (seconds) since timer was started"""

    def stop(self) -> float:
        """stop timer if still running and return time between start and initial stop"""

@forward
class ModelSettings:
    """The set of parameters setting the thermodynamic conditions of the
    design
    """
    # parameters: 'model.ParameterFile'
    dangle_type: str
    # conditions: 'model.Conditions'

    def __init__(self, material='RNA', ensemble='stacking', temperature=None, sodium=1.0, magnesium=0.0, _fun_=None):
        """create model settings"""
        if temperature == None:
            temperature = constants.default_temperature
        _fun_(self, material, ensemble, temperature, sodium, magnesium)


class StopCondition:
    '''Condition class which says:
    - Emit a checkpoint if done = True for a design
    - Interrupt the design if self.cancel() is called
    - Otherwise do nothing.
    '''
    def __init__(self):
        self.stop = False

    def cancel(self):
        self.stop = True

    def __call__(self, stats, timer, done) -> int:
        if done:
            return +1
        elif self.stop:
            return -1
        else:
            return 0


class TimeInterval(StopCondition):
    def __init__(self, interval: float):
        super().__init__()
        self.interval = interval

    def __call__(self, stats, timer, done) -> int:
        stop = super().__call__(stats, timer, done)
        if stop: return stop
        current = timer.elapsed()
        if timer.elapsed() > self.interval:
            self.last = current
            return +1
        return 0


class MutationInterval(StopCondition):
    def __init__(self, interval: int):
        super().__init__()
        self.interval = interval
        self.last = 0

    def __call__(self, stats, timer, done) -> int:
        stop = super().__call__(stats, timer, done)
        if stop: return stop
        current = stats.num_leaf_evaluations
        if current - self.last > self.interval:
            self.last = current
            return +1
        return 0


class WriteToFileCheckpoint:
    def __init__(self, path, timespec='seconds'):
        self.path = pathlib.Path(path)
        self.timespec = timespec

    def __call__(self, result):
        # time = datetime.datetime.now().isoformat(timespec=self.timespec)
        time = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        self.path.mkdir(parents=True, exist_ok=True)
        path = self.path/'{}.json'.format(time)
        path.write_text(result.to_json().dump(indent=4))


import pytest

import os
import sys

from pathlib import Path

sys.path.append(os.path.join(Path(__file__).resolve().parent, "build"))

from libewma_py import Ewma


def test_ewma(snapshot):
    slow = Ewma(0.95, 0.1)
    fast = Ewma(0.95 * 0.75, 0.1)

    for i in range(0, 5):
        slow.add(0.2)  # latency 0.2 now
        fast.add(0.2)
    assert slow.estimate() == snapshot
    assert fast.estimate() == snapshot
    assert slow.estimate() < fast.estimate()

    for i in range(5, 10):
        slow.add(0.1)  # latency 0.1 again
        fast.add(0.1)
    assert slow.estimate() == snapshot
    assert fast.estimate() == snapshot

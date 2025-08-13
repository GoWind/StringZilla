#!/usr/bin/env python3
"""
Test suite for StringZillas parallel algorithms module.
Tests with Python lists, NumPy arrays, Apache Arrow columns, and StringZilla Strs types.
To run for the CPU backend:

    uv pip install numpy pyarrow pytest pytest-repeat
    SZ_TARGET=stringzillas-cpus uv pip install -e . --force-reinstall --no-build-isolation
    uv run --no-project python -c "import stringzillas; print(stringzillas.__capabilities__)"
    uv run --no-project python -m pytest scripts/test_stringzillas.py -s -x

To run for the CUDA backend:

    uv pip install numpy pyarrow pytest pytest-repeat
    SZ_TARGET=stringzillas-cuda uv pip install -e . --force-reinstall --no-build-isolation
    uv run --no-project python -c "import stringzillas; print(stringzillas.__capabilities__)"
    uv run --no-project python -m pytest scripts/test_stringzillas.py -s -x
"""

from random import choice, randint
from string import ascii_lowercase
from typing import Optional, Literal

import pytest

import stringzilla as sz
import stringzillas as szs
from stringzilla import Str, Strs

# NumPy is available on most platforms and is required for most tests.
# When using PyPy on some platforms NumPy has internal issues, that will
# raise a weird error, not an `ImportError`. That's why we intentionally
# use a naked `except:`. Necessary evil!
try:
    import numpy as np

    numpy_available = True
except:
    # NumPy is not installed, most tests will be skipped
    numpy_available = False


# PyArrow is not available on most platforms.
# When using PyPy on some platforms PyArrow has internal issues, that will
# raise a weird error, not an `ImportError`. That's why we intentionally
# use a naked `except:`. Necessary evil!
try:
    import pyarrow as pa

    pyarrow_available = True
except:
    # PyArrow is not installed, most tests will be skipped
    pyarrow_available = False


def test_library_properties():
    assert len(sz.__version__.split(".")) == 3, "Semantic versioning must be preserved"
    assert "serial" in sz.__capabilities__, "Serial backend must be present"

    # Test StringZillas properties
    assert len(szs.__version__.split(".")) == 3, "Semantic versioning must be preserved"
    assert "serial" in szs.__capabilities__, "Serial backend must be present"


DeviceName = Literal["default", "cpu_cores", "gpu_device"]
DEVICE_NAMES = ["default", "cpu_cores", "gpu_device"] if "cuda" in szs.__capabilities__ else ["default", "cpu_cores"]


def device_scope_and_capabilities(device: DeviceName):
    """Create a DeviceScope based on the specified device type."""
    if device == "default":
        return szs.DeviceScope(), ("serial",)
    elif device == "cpu_cores":
        return szs.DeviceScope(cpu_cores=2), ("serial", "parallel")
    elif device == "gpu_device":
        return szs.DeviceScope(gpu_device=0), ("cuda",)
    else:
        raise ValueError(f"Unknown device type: {device}")


InputSizeConfig = Literal["one-large", "few-big", "many-small"]
INPUT_SIZE_CONFIGS = ["one-large", "few-big", "many-small"]


def generate_string_batches(config: InputSizeConfig):
    """Generate string batches based on the specified configuration.

    Returns:
        tuple: (batch_size, min_length, max_length) parameters for generating test strings
    """
    if config == "one-large":
        return 1, 50, 1024  # Single pair of long strings
    elif config == "few-big":
        return 7, 30, 128  # Few pairs of medium strings
    elif config == "many-small":
        return 1000, 10, 30  # Many pairs of short strings
    else:
        raise ValueError(f"Unknown input size config: {config}")


def get_random_string_batch(config: InputSizeConfig):
    """Generate two batches of random strings based on the configuration."""
    batch_size, min_len, max_len = generate_string_batches(config)

    # Generate random lengths for each string in the batch
    a_lengths = [randint(min_len, max_len) for _ in range(batch_size)]
    b_lengths = [randint(min_len, max_len) for _ in range(batch_size)]

    a_batch = [get_random_string(length=length) for length in a_lengths]
    b_batch = [get_random_string(length=length) for length in b_lengths]

    return a_batch, b_batch


def test_device_scope():
    """Test DeviceScope for execution context control."""

    default_scope = szs.DeviceScope()
    assert default_scope is not None

    scope_multi = szs.DeviceScope(cpu_cores=4)
    assert scope_multi is not None

    if "cuda" in szs.__capabilities__:
        try:
            scope_gpu = szs.DeviceScope(gpu_device=0)
            assert scope_gpu is not None
        except RuntimeError:
            # GPU capability is reported but device initialization failed
            pass
    else:
        with pytest.raises(RuntimeError):
            szs.DeviceScope(gpu_device=0)

    # Test single-threaded execution
    scope_single = szs.DeviceScope(cpu_cores=1)
    assert scope_single is not None

    # Invalid arguments
    with pytest.raises(ValueError):
        szs.DeviceScope(cpu_cores=4, gpu_device=0)  # Can't specify both

    with pytest.raises(TypeError):
        szs.DeviceScope(cpu_cores="invalid")

    with pytest.raises(TypeError):
        szs.DeviceScope(gpu_device="invalid")


def get_random_string(
    length: Optional[int] = None,
    variability: Optional[int] = None,
) -> str:
    if length is None:
        length = randint(3, 300)
    if variability is None:
        variability = len(ascii_lowercase)
    return "".join(choice(ascii_lowercase[:variability]) for _ in range(length))


def is_equal_strings(native_strings, big_strings):
    for native_slice, big_slice in zip(native_strings, big_strings):
        assert native_slice == big_slice, f"Mismatch between `{native_slice}` and `{str(big_slice)}`"


@pytest.mark.skipif(not numpy_available, reason="NumPy is not installed")
def baseline_levenshtein_distance(s1, s2) -> int:
    """
    Compute the Levenshtein distance between two strings.
    """

    # Create a matrix of size (len(s1)+1) x (len(s2)+1)
    matrix = np.zeros((len(s1) + 1, len(s2) + 1), dtype=int)

    # Initialize the first column and first row of the matrix
    for i in range(len(s1) + 1):
        matrix[i, 0] = i
    for j in range(len(s2) + 1):
        matrix[0, j] = j

    # Compute Levenshtein distance
    for i in range(1, len(s1) + 1):
        for j in range(1, len(s2) + 1):
            if s1[i - 1] == s2[j - 1]:
                cost = 0
            else:
                cost = 1
            matrix[i, j] = min(
                matrix[i - 1, j] + 1,  # Deletion
                matrix[i, j - 1] + 1,  # Insertion
                matrix[i - 1, j - 1] + cost,  # Substitution
            )

    # Return the Levenshtein distance
    return matrix[len(s1), len(s2)]


@pytest.mark.repeat(100)
@pytest.mark.parametrize("max_edit_distance", [150])
def test_levenshtein_distance_insertions(max_edit_distance: int):
    # Create a new string by slicing and concatenating
    def insert_char_at(s, char_to_insert, index):
        return s[:index] + char_to_insert + s[index:]

    binary_engine = szs.LevenshteinDistances()

    a = get_random_string(length=20)
    b = a
    for i in range(max_edit_distance):
        source_offset = randint(0, len(ascii_lowercase) - 1)
        target_offset = randint(0, len(b) - 1)
        b = insert_char_at(b, ascii_lowercase[source_offset], target_offset)
        a_strs = Strs([a])
        b_strs = Strs([b])
        results = binary_engine(a_strs, b_strs)
        assert len(results) == 1, "Binary engine should return a single distance"
        assert results[0] == [i + 1], f"Edit distance mismatch after {i + 1} insertions: {a} -> {b}"


@pytest.mark.parametrize("device_name", DEVICE_NAMES)
def test_levenshtein_distances_with_simple_cases(device_name: DeviceName):

    device_scope, capabilities = device_scope_and_capabilities(device_name)
    binary_engine = szs.LevenshteinDistances(capabilities=capabilities)

    def binary_distance(a: str, b: str) -> int:
        a_strs = Strs([a])
        b_strs = Strs([b])
        results = binary_engine(a_strs, b_strs, device=device_scope)
        assert len(results) == 1, "Binary engine should return a single distance"
        return results[0]

    assert binary_distance("hello", "hello") == 0
    assert binary_distance("hello", "hell") == 1
    assert binary_distance("", "") == 0
    assert binary_distance("", "abc") == 3
    assert binary_distance("abc", "") == 3
    assert binary_distance("abc", "ac") == 1, "one deletion"
    assert binary_distance("abc", "a_bc") == 1, "one insertion"
    assert binary_distance("abc", "adc") == 1, "one substitution"
    assert binary_distance("ggbuzgjux{}l", "gbuzgjux{}l") == 1, "one insertion (prepended)"
    assert binary_distance("abcdefgABCDEFG", "ABCDEFGabcdefg") == 14


@pytest.mark.parametrize("device_name", DEVICE_NAMES)
def test_levenshtein_distances_utf8_with_simple_cases(device_name: DeviceName):

    if device_name == "cuda":
        pytest.skip("CUDA backend does not support custom gaps in UTF-8 Levenshtein distances")

    device_scope, capabilities = device_scope_and_capabilities(device_name)
    unicode_engine = szs.LevenshteinDistancesUTF8(capabilities=capabilities)

    def unicode_distance(a: str, b: str) -> int:
        a_strs = Strs([a])
        b_strs = Strs([b])
        results = unicode_engine(a_strs, b_strs, device=device_scope)
        assert len(results) == 1, "Unicode engine should return a single distance"
        return results[0]

    assert unicode_distance("hello", "hell") == 1, "no unicode symbols, just ASCII"
    assert unicode_distance("𠜎 𠜱 𠝹 𠱓", "𠜎𠜱𠝹𠱓") == 3, "add 3 whitespaces in Chinese"
    assert unicode_distance("💖", "💗") == 1

    assert unicode_distance("αβγδ", "αγδ") == 1, "insert Beta"
    assert unicode_distance("école", "école") == 2, "etter 'é' as 1 character vs 'e' + '´'"
    assert unicode_distance("façade", "facade") == 1, "'ç' with cedilla vs. plain"
    assert unicode_distance("Schön", "Scho\u0308n") == 2, "'ö' represented as 'o' + '¨'"
    assert unicode_distance("München", "Muenchen") == 2, "German with umlaut vs. transcription"
    assert unicode_distance("こんにちは世界", "こんばんは世界") == 2, "Japanese greetings"


@pytest.mark.parametrize("device_name", DEVICE_NAMES)
def test_levenshtein_distances_with_custom_gaps(device_name: DeviceName):

    mismatch: int = 4
    opening: int = 3
    extension: int = 2

    device_scope, capabilities = device_scope_and_capabilities(device_name)
    binary_engine = szs.LevenshteinDistances(
        open=opening, extend=extension, mismatch=mismatch, capabilities=capabilities
    )

    def binary_distance(a: str, b: str) -> int:
        a_strs = Strs([a])
        b_strs = Strs([b])
        results = binary_engine(a_strs, b_strs, device=device_scope)
        assert len(results) == 1, "Binary engine should return a single distance"
        return results[0]

    assert binary_distance("hello", "hello") == 0
    assert binary_distance("hello", "hell") == opening
    assert binary_distance("", "") == 0
    assert binary_distance("", "abc") == opening + 2 * extension
    assert binary_distance("abc", "") == opening + 2 * extension
    assert binary_distance("abc", "ac") == opening, "one deletion"
    assert binary_distance("abc", "a_bc") == opening, "one insertion"
    assert binary_distance("abc", "adc") == mismatch, "one substitution"
    assert binary_distance("ggbuzgjux{}l", "gbuzgjux{}l") == opening, "one insertion (prepended)"
    assert binary_distance("abcdefgABCDEFG", "ABCDEFGabcdefg") == min(14 * mismatch, 2 * opening + 12 * extension)


@pytest.mark.parametrize("device_name", DEVICE_NAMES)
def test_levenshtein_distances_utf8_with_custom_gaps(device_name: DeviceName):

    if device_name == "cuda":
        pytest.skip("CUDA backend does not support custom gaps in UTF-8 Levenshtein distances")

    mismatch: int = 4
    opening: int = 3

    device_scope, capabilities = device_scope_and_capabilities(device_name)
    unicode_engine = szs.LevenshteinDistancesUTF8(gap=opening, mismatch=mismatch, capabilities=capabilities)

    def unicode_distance(a: str, b: str) -> int:
        a_strs = Strs([a])
        b_strs = Strs([b])
        results = unicode_engine(a_strs, b_strs, device=device_scope)
        assert len(results) == 1, "Unicode engine should return a single distance"
        return results[0]

    assert unicode_distance("hello", "hell") == opening, "no unicode symbols, just ASCII"
    assert unicode_distance("𠜎 𠜱 𠝹 𠱓", "𠜎𠜱𠝹𠱓") == 3 * opening, "add 3 whitespaces in Chinese"
    assert unicode_distance("💖", "💗") == 1 * mismatch

    assert unicode_distance("αβγδ", "αγδ") == opening, "insert Beta"
    assert unicode_distance("école", "école") == mismatch + opening, "etter 'é' as 1 character vs 'e' + '´'"
    assert unicode_distance("façade", "facade") == mismatch, "'ç' with cedilla vs. plain"
    assert unicode_distance("Schön", "Scho\u0308n") == mismatch + opening, "'ö' represented as 'o' + '¨'"
    assert unicode_distance("München", "Muenchen") == mismatch + opening, "German with umlaut vs. transcription"
    assert unicode_distance("こんにちは世界", "こんばんは世界") == min(2 * mismatch, 4 * opening), "Japanese greetings"


@pytest.mark.repeat(10)
@pytest.mark.parametrize("config", INPUT_SIZE_CONFIGS)
@pytest.mark.skipif(not numpy_available, reason="NumPy is not installed")
def test_levenshtein_distance_random(config: InputSizeConfig):
    a_batch, b_batch = get_random_string_batch(config)

    baselines = np.array([baseline_levenshtein_distance(a, b) for a, b in zip(a_batch, b_batch)])
    engine = szs.LevenshteinDistances()

    # Convert to Strs objects
    a_strs = Strs(a_batch)
    b_strs = Strs(b_batch)
    results = engine(a_strs, b_strs)

    np.testing.assert_array_equal(results, baselines, "Edit distances do not match")


@pytest.mark.repeat(10)
@pytest.mark.parametrize("config", INPUT_SIZE_CONFIGS)
@pytest.mark.skipif(not numpy_available, reason="NumPy is not installed")
def test_needleman_wunsch_vs_levenshtein_random(config: InputSizeConfig):
    """Test Needleman-Wunsch global alignment scores against Levenshtein distances with random strings."""

    a_batch, b_batch = get_random_string_batch(config)

    character_substitutions = np.zeros((256, 256), dtype=np.int8)
    character_substitutions.fill(-1)
    np.fill_diagonal(character_substitutions, 0)

    baselines = [-baseline_levenshtein_distance(a, b) for a, b in zip(a_batch, b_batch)]
    engine = szs.NeedlemanWunsch(substitution_matrix=character_substitutions, open=-1, extend=-1)

    # Convert to Strs objects
    a_strs = Strs(a_batch)
    b_strs = Strs(b_batch)
    results = engine(a_strs, b_strs)

    np.testing.assert_array_equal(results, baselines, "Edit distances do not match")


def test_fingerprints():
    """Test Fingerprints and FingerprintsUTF8 basic functionality."""

    engine = szs.Fingerprints()
    utf8_engine = szs.FingerprintsUTF8()

    # Basic functionality
    assert engine([]) == []
    assert utf8_engine([]) == []

    test_strings = ["hello", "world", "hello"]
    results = engine(test_strings)
    assert len(results) == 3
    assert results[0] == results[2], "Identical strings should produce identical fingerprints"
    assert results[0] != results[1], "Different strings should produce different fingerprints"

    # Unicode handling
    unicode_strings = ["café", "世界", "🌟"]
    utf8_results = utf8_engine(unicode_strings)
    assert len(utf8_results) == 3
    assert (
        len(set(tuple(fp) if hasattr(fp, "__iter__") else fp for fp in utf8_results)) == 3
    ), "Unicode strings should produce unique fingerprints"


@pytest.mark.repeat(5)
@pytest.mark.parametrize("batch_size", [1, 10, 100])
def test_fingerprints_random(batch_size: int):
    """Test fingerprinting with random strings."""

    engine = szs.Fingerprints()
    batch = [get_random_string(length=randint(5, 50)) for _ in range(batch_size)]

    results = engine(batch)
    assert len(results) == batch_size

    # Verify consistency
    results_repeated = engine(batch)
    assert results == results_repeated, "Same input should produce same fingerprints"


if __name__ == "__main__":
    import sys

    sys.exit(pytest.main(["-x", "-s", __file__]))

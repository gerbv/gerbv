# Gerbv Test Suite

## Prerequisites

Install ImageMagick (used for pixel-by-pixel image comparison):

```sh
sudo apt install imagemagick
```

## Running Tests

### Via CTest (primary method)

After building with the `linux-gnu-gcc` preset, run the full regression suite:

```sh
ctest --preset linux-gnu-gcc
```

Or, if you prefer to point directly at the build directory:

```sh
ctest --test-dir build -C Debug
```

### Running a specific sub-test by name

The CTest integration runs all regression tests as a single test (`gerbv_regression_tests`).
To run only one specific sub-test, invoke the script directly with its name:

```sh
GERBV=../build/src/Debug/gerbv \
GERBV_SCHEMEINIT=../src \
LD_LIBRARY_PATH=../build/src/Debug:$LD_LIBRARY_PATH \
bash run_tests.sh LimeSDR
```

**Note:** When running via CTest, `GERBV`, `GERBV_SCHEMEINIT`, and `LD_LIBRARY_PATH` are set
automatically. For direct shell invocation (both full run and single sub-test) they must be
set manually as shown above.

## Understanding Failures

Golden reference images in `golden/` were generated with a specific version of Cairo. Running
the test suite on a machine with a different Cairo version may produce pixel-level differences
in anti-aliased output, causing otherwise-correct tests to be reported as FAILED. These are
false positives — inspect the diff images in `mismatch/` to determine whether the rendering
change is intentional.

## Regenerating Golden Images

After verifying that a rendering change is correct, regenerate the golden reference for that
test (do **not** blindly regenerate — this defeats the purpose of the tests):

```sh
./run_tests.sh --regen <testname>
```

Then inspect the new PNG in `golden/` to confirm it looks correct before committing it.

## Adding a New Test

1. Create a *small* input file (RS274-X, NC-drill, etc.) in `inputs/` that exercises one
   specific aspect of the file format.
2. Add an entry to `tests.list` following the existing format.
3. Generate the golden reference:
   ```sh
   ./run_tests.sh --regen <new_test_name>
   ```
4. Inspect `golden/<new_test_name>.png` to confirm the output is correct.
5. Commit both the input file and the golden PNG.

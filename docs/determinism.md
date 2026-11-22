# Determinism notes

Idem promises that a recording made by any supported build replays bit-for-bit on
any other supported build. "Supported" currently means x86-64 with GCC 14,
Clang 19 and MSVC 2022, which CI checks on every push. This file lists what had to
be true for that to hold, roughly in order of how often each one bites.

## 1. No floating-point contraction

`a * b + c` may be compiled into a single fused multiply-add, which rounds once
instead of twice. Both results are correct; they are just different.

GCC contracts by default whenever the target has FMA (`-march=haswell` or newer,
or `-march=native` on any recent CPU). Clang does the same within a single
expression. MSVC does not contract under `/fp:precise` unless `/fp:contract` is
given. The build therefore passes `-ffp-contract=off` on GCC and Clang, and it is
a `PUBLIC` option on the library target so that inline math in headers is
compiled the same way in every consumer.

What happens without it, measured on this code base with GCC 14:

| What was built with `-march=haswell -ffp-contract=fast` | First divergent tick |
|---|---|
| everything | 0, the generated scene already differs (`lo + (hi - lo) * u`) |
| only `simulation.cpp` | 1, the first contact solve (`dot`) |

`-march=native` with `-ffp-contract=off` produces the same hashes as a baseline
x86-64 build.

## 2. No fast-math

`-ffast-math` and `/fp:fast` allow reassociation, reciprocal approximations and
assuming no NaNs. `math.hpp` refuses to compile if `__FAST_MATH__` or
`_M_FP_FAST` is defined.

## 3. No excess precision

On 32-bit x86 with the x87 FPU, intermediates are kept in 80-bit registers and
rounded when spilled, so results depend on register allocation. `math.hpp`
asserts `FLT_EVAL_METHOD == 0`.

## 4. Only correctly rounded operations

IEEE 754 requires `+ - * /` and `sqrt` to be correctly rounded, so every
conforming implementation agrees on them. It does not require that of `sin`,
`exp`, `pow` or `cbrt`, and glibc, the MSVC CRT and Apple's libm do return
different last bits. The simulation uses nothing beyond the basic operations and
`std::sqrt`. The world size needed a cube root, so it is computed with an integer
binary search instead of `std::cbrt`.

## 5. No implementation-defined library behaviour

`std::mt19937` is specified exactly, but `std::uniform_real_distribution` and
friends are not, and libstdc++ and the MSVC STL produce different sequences from
the same engine. `rng.hpp` implements SplitMix64, xoshiro256** and the
float/integer mappings directly.

Iteration order of `std::unordered_map` differs between standard libraries too;
nothing on the simulation path iterates one.

## 6. No unspecified evaluation order

```cpp
make_vec(rng.next(), rng.next(), rng.next()); // order unspecified, and GCC and MSVC disagree
Vec3{rng.next(), rng.next(), rng.next()};     // braced init: left to right, guaranteed
```

## 7. Order-independent results from order-dependent algorithms

The contact solver is Gauss-Seidel: each contact sees the position and velocity
corrections of the contacts solved before it, so the order of pairs is part of
the result. The three broad-phases emit pairs in completely different orders (and
the BVH's tree shape is itself an implementation detail), so pairs are sorted
before solving. The test suite checks that brute force, grid and BVH produce the
same state hash on every tick.

This is also what makes a parallel broad-phase safe to add later: only the pair
set matters, not the order threads produce it in.

## 8. The hash looks at bits, not values

The state hash reads the raw bit patterns of positions and velocities, so `0.0f`
and `-0.0f` are different states. A value-based comparison would hide exactly the
kind of divergence that grows into a visible one a few hundred ticks later.

## 9. Explicit byte order on disk

Recordings are little-endian regardless of the host, with floats stored as their
IEEE-754 bit patterns.

## Known gaps

- **FPU control state.** Results assume the default rounding mode and no
  flush-to-zero / denormals-are-zero. A plugin or graphics driver that changes
  MXCSR on the simulation thread would break determinism silently. A production
  version would set and verify the control word at the start of each step.
- **Other architectures.** AArch64 is IEEE-754 conformant and should work with
  contraction disabled, but it is not tested.
- **Compiler upgrades.** The golden-hash test pins one long run. If it changes
  after a toolchain upgrade, that upgrade changed simulation results.

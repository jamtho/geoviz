# Agent Guidelines for geoviz

## Tooling

- Use **uv** for all Python tooling (test data generation, scripts, etc.)
- Use **CMake** for the C build system

## Code Style

- Use **type hints** extensively in all Python code — every function signature, every variable where the type isn't immediately obvious
- C code follows C11 with clear, explicit types and `const` where appropriate

## Testing

- Write **lots of high-quality, useful tests**
- Unit tests for all pure functions (colormap lookups, coordinate transforms, spec parsing, etc.)
- Integration tests for the data loading pipeline
- Property-based tests where applicable (e.g., coordinate round-trips)
- Test edge cases: empty datasets, single points, extreme coordinates, missing optional fields
- Use **pytest** for Python test code
- C tests use a simple test harness (assert-based, one test binary per module)

## Version Control

- **Git commit frequently** — after each logical unit of work
- Write clear, concise commit messages describing *why*, not just *what*
- Keep commits atomic: one concern per commit

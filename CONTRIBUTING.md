# Contributing to Astra

First off, thank you for considering contributing to Astra! It's people like you that make Astra such a great language.

## Getting Started

1. **Fork the Repository**: Start by forking the [Astra repository](#) to your GitHub account.
2. **Clone the Repo**: 
   ```bash
   git clone https://github.com/<your-username>/Astra_MY_Language.git
   cd Astra_MY_Language
   ```
3. **Build the Project**: Astra is built with C. You'll need `make` and `clang` or `gcc`.
   ```bash
   make
   ```

## Development Workflow

1. Create a branch for your feature or bug fix:
   ```bash
   git checkout -b feature/my-new-feature
   ```
2. Write your code.
3. Add or update tests as necessary in the `tests/` directory.
4. Run the test suite:
   ```bash
   make test
   ```
   *Ensure all tests pass before submitting a pull request.*
5. Commit your changes. Write clear, descriptive commit messages.
6. Push to your fork:
   ```bash
   git push origin feature/my-new-feature
   ```
7. Open a Pull Request from your fork to the main Astra repository.

## Submitting Pull Requests

- Give your PR a descriptive title.
- Use our Pull Request template to describe the changes, motivation, and any testing performed.
- Ensure your code adheres to our project's coding style (e.g., standard C conventions).

## Reporting Bugs and Feature Requests

Please use the issue tracker provided on GitHub. We've set up issue templates to help you provide the right information.
- For **Bugs**, please include steps to reproduce, expected behavior, and the OS version you're running on.
- For **Feature Requests**, explain why the feature is needed and optionally provide how it might look syntax-wise.

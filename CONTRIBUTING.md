# Contributing to jmraidstatus

Thank you for your interest in contributing to jmraidstatus!

## Getting Started

### Prerequisites
- GCC compiler (or compatible C compiler)
- GNU Make
- Linux system (or WSL on Windows)
- JMicron RAID controller for testing (JMB394, JMB393, etc.)

### Building from Source

```bash
git clone https://github.com/jamietre/jm-raid-status.git
cd jm-raid-status
make
```

The binary will be built to `bin/jmraidstatus`.

### Testing

```bash
# Test with your JMicron device
sudo bin/jmraidstatus /dev/sdX

# Test various output modes
sudo bin/jmraidstatus --full /dev/sdX
sudo bin/jmraidstatus --json /dev/sdX
```

## Development Workflow

1. **Fork the repository** on GitHub
2. **Clone your fork** locally
3. **Create a feature branch**: `git checkout -b feature/my-new-feature`
4. **Make your changes** and test thoroughly
5. **Commit your changes**: `git commit -am 'Add some feature'`
6. **Push to your fork**: `git push origin feature/my-new-feature`
7. **Create a Pull Request** on GitHub

## Code Style

- Follow existing code style and formatting
- Use consistent indentation (4 spaces)
- Add comments for complex logic
- Keep functions focused and manageable in size

## Directory Structure

```
jmraidstatus/
├── src/              # Source code
├── bin/              # Build output (not in repo)
├── tests/            # Test utilities
├── .github/          # GitHub Actions workflows
└── docs/             # Documentation (if needed)
```

## Adding New Features

When adding features:
1. Update relevant documentation (README.md, QUICK_START.md)
2. Add error handling for failure cases
3. Test with actual hardware if possible
4. Consider backward compatibility

## Reporting Bugs

When reporting bugs, please include:
- jmraidstatus version (`jmraidstatus --version`)
- Linux distribution and kernel version
- JMicron controller model (from `lspci`)
- Error messages and output
- Steps to reproduce

## Questions?

Feel free to open an issue for:
- Bug reports
- Feature requests
- Questions about usage
- Documentation improvements

## License

By contributing, you agree that your contributions will be licensed under the same MIT license as the project.

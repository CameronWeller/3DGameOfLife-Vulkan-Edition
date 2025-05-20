#!/bin/bash

# Create .git/hooks directory if it doesn't exist
mkdir -p .git/hooks

# Create pre-commit hook
cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash

# Run clang-format on staged files
for file in $(git diff --cached --name-only --diff-filter=ACMR | grep -E '\.(cpp|h)$')
do
    clang-format -i "$file"
    git add "$file"
done

# Run clang-tidy on staged files
for file in $(git diff --cached --name-only --diff-filter=ACMR | grep -E '\.(cpp|h)$')
do
    clang-tidy "$file" -checks=*,-fuchsia-*,-google-*,-zircon-*,-abseil-*,-modernize-use-trailing-return-type
done
EOF

# Make the hook executable
chmod +x .git/hooks/pre-commit

echo "Pre-commit hooks installed successfully!" 
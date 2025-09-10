#!/bin/bash

# Prompt for input
read -p "From module: " from_module
read -p "To module: " to_module
read -p "String resource name: " string_name

# Find all strings.xml files in the from module
strings_files=$(find "$from_module/src/main/res" -type f -name "strings.xml")

# Loop through each strings.xml file
for from_file in $strings_files; do
    # Extract the exact <string> line (preserves backslashes)
    string_line=$(grep -oP "<string name=\"$string_name\">.*?</string>" "$from_file")

    # Skip if string not found
    if [[ -z "$string_line" ]]; then
        continue
    fi

    # Determine the value folder path
    relative_path="${from_file#$from_module/src/main/res/}"  # e.g., values-de/strings.xml
    to_file="$to_module/src/main/res/$relative_path"

    # Ensure the destination directory exists
    to_dir=$(dirname "$to_file")
    mkdir -p "$to_dir"

    # Add or create the string in the destination file
    if [[ -f "$to_file" ]]; then
        # Add string if not already present
        if ! grep -q "name=\"$string_name\"" "$to_file"; then
            # Safely insert the raw string line
            sed -i.bak "/<\/resources>/i \ \ \ \ $string_line" "$to_file"
            echo "Added '$string_name' to $to_file"
        else
            echo "'$string_name' already exists in $to_file"
        fi
    else
        # Create a new strings.xml
        cat > "$to_file" <<EOF
<resources>
    $string_line
</resources>
EOF
        echo "Created $to_file with '$string_name'"
    fi

    # Remove the string from the source file
    sed -i.bak "/<string name=\"$string_name\">.*<\/string>/d" "$from_file"

    # Cleanup backup files
    rm -f "$from_file.bak" "$to_file.bak"
done

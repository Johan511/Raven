#!/bin/bash

# Directory to clean (current directory)
DIR="."

# Files to keep
KEEP_FILES=("updates.md" "updates.tex")

#generate tex file
pandoc updates.md -s -V geometry:margin=1in --listings --include-in-header=cpp-header.tex -o updates.tex 

# pandoc sometimes does some weird async stuff to generate pdf from latex
sleep 2

# Loop through all files in the directory that start with "updates"
for FILE in "$DIR"/updates*; do
    BASENAME=$(basename "$FILE")
    
    # Check if the file is not in the list of files to keep
    if [[ ! " ${KEEP_FILES[@]} " =~ " ${BASENAME} " ]]; then
        # Delete the file
        rm "$FILE"
        echo "Deleted $FILE"
    fi
done

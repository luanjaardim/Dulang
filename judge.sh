#!/usr/bin/env sh

operation=$1
dir=$(pwd)/$2

if [ "$operation" = "run" ]; then
    echo "Run Testing"
    if [ -d "$dir" ]; then
        for file in $dir/*.dulan
        do
            if [ -f "$file" ] && [ -f "$file.answ" ]; then
                fileToPrint=${file##*/}
                echo "Compiling $fileToPrint"
                ./dulang "$file"
                wait $! #wait for the last command

                #remove the .dulan from file
                exec=${file%.dulan}
                if [ -f "$exec" ]; then
                    echo "Testing $fileToPrint with $fileToPrint.answ"
                    "/$exec" > "$dir/tmp"
                    wait $!
                    difference=$(diff -u "$file.answ" "$dir/tmp")
                    if [ "$difference" = "" ]; then
                        echo "Test passed"
                    else
                        echo "Test failed"
                        echo "$difference"
                        rm "$dir/tmp"
                        rm "$exec.asm"
                        rm "$exec"
                        exit 1
                    fi
                    rm "$exec.asm"
                    rm "$exec"
                else
                    echo "Failed to compile $file"
                    if [ -f "$dir/tmp" ]; then
                        rm "$dir/tmp"
                    fi
                    exit 1
                fi
            fi
        done

        if [ -f "$dir/tmp" ]; then
            rm $dir/tmp
        fi
    else
        echo "Directory does not exist"
    fi

elif [ "$operation" = "update" ]; then
    echo "Update file answers"

    if [ -d "$dir" ]; then
        for file in $dir/*.dulan
        do
            if [ -f "$file" ]; then
                echo "Compiling $file"
                ./dulang $file
                wait $!

                #remove the .dulan from file
                exec=${file%.dulan}
                if [ -f "$exec" ]; then
                    echo "Updating $file.answ"
                    "/$exec" > "$file.answ" #wait last command to finish
                    wait $!
                    rm "$exec.asm"
                    rm "$exec"
                else
                    echo "Failed to compile $file"
                    exit 1
                fi
            fi
        done
    else
        echo "$dir"
        echo "Directory does not exist"
    fi
else
    echo "Usage: sh judge.sh <operation> <directory>"
    echo "Operations:"
    echo "  run: test all the files in the directory"
    echo "  update: update all the .answ files in the directory"
fi

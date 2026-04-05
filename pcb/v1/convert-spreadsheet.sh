#!/bin/sh

INPUT="kicadpartsplacer-report.ods"

convert() {
    /Applications/LibreOffice.app/Contents/MacOS/soffice --headless --convert-to csv "$INPUT"
}

case "$1" in
    once)
        convert
        ;;
    watch)
        convert
        echo "Watching for changes in $INPUT..."
        fswatch -o "$INPUT" | xargs -n1 -I{} sh -c "
            echo \"Converting $INPUT...\" && /Applications/LibreOffice.app/Contents/MacOS/soffice --headless --convert-to csv \"$INPUT\"
        "
        ;;
    *)
        echo "Usage: $0 {once|watch}"
        exit 1
        ;;
esac

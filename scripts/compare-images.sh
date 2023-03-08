[[ -f output-images/.venv/Scripts/activate ]] && source output-images/.venv/Scripts/activate
[[ -f output-images/.venv/bin/activate ]] && source output-images/.venv/bin/activate

python scripts/compare-images.py

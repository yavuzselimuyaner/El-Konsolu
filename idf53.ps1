# Activates ESP-IDF 5.3.6 for Retro-Go in the current shell: . .\idf53.ps1
$env:IDF_TOOLS_PATH = 'C:\esp\idf53-tools'
$env:IDF_PATH = 'C:\esp\v5.3.6\esp-idf'
$env:IDF_PYTHON_ENV_PATH = 'C:\esp\idf53-tools\python_env\idf5.3_py3.12_env'
$py312 = (& py -3.12 -c "import sys,os;print(os.path.dirname(sys.executable))").Trim()
$env:PATH = "$py312;$py312\Scripts;$env:PATH"
. "$env:IDF_PATH\export.ps1" | Out-Null

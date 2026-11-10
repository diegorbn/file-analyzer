# Demostración estática

En Linux/WSL con GCC, desde este directorio:

```sh
gcc -std=c11 -O2 -Wall -Wextra -o file-analyzer main.c txt.c csv.c log.c
./file-analyzer
```

Escribe `list`, `analyze examples/ejemplo.txt`, `analyze examples/ejemplo.csv`, `analyze examples/ejemplo.log`, `help` o `quit`. Los tres analizadores están enlazados en el ejecutable; no se necesitan complementos.

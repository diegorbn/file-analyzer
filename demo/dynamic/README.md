# Demostración dinámica

En Linux/WSL con GCC, trabaja desde este directorio (el directorio de complementos predeterminado es `./plugins`):

```sh
gcc -std=c11 -O2 -Wall -Wextra -o file-analyzer main.c -ldl
gcc -std=c11 -O2 -Wall -Wextra -fPIC -shared -o txt_plugin.so txt_plugin.c
gcc -std=c11 -O2 -Wall -Wextra -fPIC -shared -o csv_plugin.so csv_plugin.c
gcc -std=c11 -O2 -Wall -Wextra -fPIC -shared -o log_plugin.so log_plugin.c
mkdir -p plugins
./file-analyzer
```

Con `plugins/` vacío, escribe `list`. En otra terminal situada en este directorio, ejecuta `cp txt_plugin.so plugins/`. En la aplicación en ejecución, escribe `refresh`, `list` y `analyze examples/ejemplo.txt`. Copia CSV con `cp csv_plugin.so plugins/` y luego escribe `refresh`, `list`, `analyze examples/ejemplo.csv` y `analyze examples/ejemplo.txt`. Agrega LOG de la misma manera y analiza `examples/ejemplo.log`. Usa `help` para ver los comandos y `quit` para cerrar todos los manejadores. Un primer argumento opcional permite seleccionar otro directorio de complementos.

Usa `unload .txt` para eliminar el registro y cerrar su manejador en el proceso. Eliminar `plugins/txt_plugin.so` por sí solo no descarga una biblioteca mapeada, y `refresh` no actualiza un complemento cargado. Para actualizarlo, primero descárgalo en **todos** los procesos que lo tengan cargado (o cierra esos procesos), elimina el archivo instalado, compila el reemplazo fuera de `plugins/`, cópialo allí y ejecuta `refresh`. Nunca sobrescribas los bytes de una biblioteca mapeada. Carga solo complementos de confianza: la ABI no tiene un campo de versión y no puede validar código arbitrario.

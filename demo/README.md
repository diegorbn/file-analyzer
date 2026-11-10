# Demostraciones de File Analyzer

Ambas demostraciones son independientes y utilizan GCC en Linux/WSL. Ejecuta cada comando desde el directorio indicado para que las rutas de los ejemplos y del directorio de complementos se resuelvan correctamente.

## Versión estática

```sh
cd demo/static
gcc -std=c11 -O2 -Wall -Wextra -o file-analyzer main.c txt.c csv.c log.c
./file-analyzer
```

Los analizadores TXT, CSV y LOG están enlazados en el ejecutable. Prueba `list`, `analyze examples/ejemplo.txt`, `analyze examples/ejemplo.csv`, `analyze examples/ejemplo.log` y `quit`. Consulta el [README de la demostración estática](static/README.md) para más detalles.

## Versión dinámica

```sh
cd demo/dynamic
gcc -std=c11 -O2 -Wall -Wextra -o file-analyzer main.c -ldl
gcc -std=c11 -O2 -Wall -Wextra -fPIC -shared -o txt_plugin.so txt_plugin.c
gcc -std=c11 -O2 -Wall -Wextra -fPIC -shared -o csv_plugin.so csv_plugin.c
gcc -std=c11 -O2 -Wall -Wextra -fPIC -shared -o log_plugin.so log_plugin.c
mkdir -p plugins
./file-analyzer
```

La aplicación carga bibliotecas desde `plugins/`. Mientras la aplicación está en ejecución, copia un complemento compilado a ese directorio desde otra terminal y luego utiliza `refresh` y `list` en la aplicación. El [README de la demostración dinámica](dynamic/README.md) contiene la secuencia completa para la demostración en vivo y las instrucciones para actualizar y descargar complementos de forma segura.

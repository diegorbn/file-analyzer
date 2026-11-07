# Elaborado por: Kevin Perez, Diego Urbina, Danilo Iraheta y Alexis Merino

# Analizador de archivos

Proyecto pequeño en C para Linux que muestra cómo cargar bibliotecas compartidas como plugins. El programa ofrece un menú para analizar archivos `.txt`, `.csv` y `.log`; cada formato tiene su propio analizador en `analyzers/`. También puedes compilar el plugin de logs mientras el programa está abierto y cargarlo desde la opción de recargar complementos.

## Compilación

Ejecuta estos comandos desde la raíz del proyecto. Necesitas un compilador de C, como `gcc` o `cc`:

```sh
mkdir -p plugins
cc -std=c11 -Wall -Wextra -O2 -o file-analyzer main.c -ldl
cc -std=c11 -Wall -Wextra -O2 -fPIC -shared -o plugins/txt_plugin.so analyzers/txt_plugin.c
cc -std=c11 -Wall -Wextra -O2 -fPIC -shared -o plugins/csv_plugin.so analyzers/csv_plugin.c
cc -std=c11 -Wall -Wextra -O2 -fPIC -shared -o plugins/log_plugin.so analyzers/log_plugin.c
```

Para iniciar el programa, usa `./file-analyzer` desde la raíz del proyecto. Los archivos `.so` deben quedar en `plugins/`, porque el programa los busca ahí. Si compilas otro plugin mientras el programa está abierto, elige **Recargar complementos** en el menú para cargarlo.


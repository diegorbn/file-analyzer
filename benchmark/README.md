# Benchmark controlado

Desde este directorio en Linux/WSL con GCC, GNU Make y Python 3:

```sh
make all
make test
python3 tools/generate.py
python3 tools/benchmark.py --trials 30 --warmups 5
```

La generación crea el conjunto de datos determinista y deja intactos los archivos existentes idénticos; se niega a sobrescribir archivos diferentes. La carga de trabajo fija consta de 10 archivos TXT y 10 CSV, cada uno de 524.288 bytes. Cada proceso nuevo adquiere el analizador TXT, procesa los archivos TXT ordenados, adquiere el analizador CSV y luego procesa los archivos CSV ordenados. Ambas variantes usan los mismos analizadores y llamadas mediante punteros a funciones; la compilación proporcionada usa `-O0` para todos los programas y analizadores del benchmark. La versión estática enlaza solo TXT y CSV; la dinámica carga cada complemento una vez. El controlador instala los complementos antes de medir el tiempo y después restaura los archivos activos anteriores. Usa `python3 tools/plugins.py --target benchmark list` (o `activate txt csv`, `deactivate txt`, `reset`) para gestionar la preparación y la instalación.

Cada ejecución crea un directorio de resultados nuevo; `--output PATH` debe indicar un directorio que no exista. `raw.csv` y `warmups.csv` contienen registros separados por proceso; `summary.csv` resume solo las pruebas medidas y `metadata.json` registra la configuración y los hashes. Se comprueba la igualdad de los resultados y se verifica el contenido del conjunto de datos antes y después. Nunca se sobrescriben resultados existentes. Las ejecuciones fallidas producen `error.txt`, no un resumen válido. Los resultados históricos y los obtenidos con distintos binarios no deben combinarse.

El `elapsed_ms` externo se mide con CLOCK_MONOTONIC desde antes de fork hasta wait4 e incluye el inicio, el análisis, la escritura del informe y la salida, pero excluye Python, la compilación y el despliegue de complementos. Los valores de adquisición están en microsegundos; los de procesamiento, en milisegundos. El RSS máximo es `ru_maxrss` de wait4 en KiB e incluye páginas compartidas. Los tamaños del ejecutable y de las bibliotecas instaladas excluyen las bibliotecas del sistema. Por defecto se ejecutan primero todos los calentamientos y pruebas de la versión estática, seguidos por los de la dinámica. Cada prueba usa un proceso nuevo; el orden se registra en los metadatos. El orden por lotes puede afectar las comparaciones debido a cambios en la carga del sistema, la temperatura o frecuencia de la CPU y el estado de la caché a lo largo del tiempo. Usa `--order alternating` para intercalar las variantes como en ejecuciones anteriores. Los calentamientos y el orden alternado reducen algunos sesgos, pero la caché, la planificación y las condiciones del sistema siguen afectando las comparaciones. Los resultados con `-O0` usan binarios distintos y no deben combinarse con los anteriores obtenidos con `-O2`. Los metadatos describen las opciones proporcionadas; si se modifican variables de Make, conserva por separado el comando real de compilación. Una ejecución de prueba rápida no permite sacar conclusiones de rendimiento.

`legacy/` es una referencia histórica inactiva que incluye la aplicación y las pruebas anteriores a la reorganización. Las compilaciones y pruebas normales no la utilizan. `make clean` elimina los productos de compilación, pero conserva los conjuntos de datos, los resultados y los complementos activos.

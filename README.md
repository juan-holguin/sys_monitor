# sys_monitor

Proyecto en C que lee la información del sistema desde `/proc/stat`, `/proc/cpuinfo` y `/proc/meminfo` en Linux.  
Muestra en pantalla cada 2 segundos:

- ✅ Memoria total, disponible y usada.  
- ✅ Uso de memoria swap.  
- ✅ Información del procesador (modelo).  
- ✅ Número de núcleos detectados.  
- ✅ Carga de CPU por cada núcleo.  

---

## 🚀 Compilación

Dentro de la carpeta del proyecto, ejecutar:

```bash
gcc -o sys_monitor sys_monitor.c



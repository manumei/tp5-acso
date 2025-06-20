# TP-5 Thread Pool — Markdown Instructions

## Universidad de San Andrés  
I301 Arquitectura de Computadoras y Sistemas Operativos

---

## Objetivo

Implementar un **Thread Pool**: un patrón de diseño para gestionar un grupo fijo de hilos (workers) que ejecutan tareas concurrentemente, usando un **dispatcher** dedicado y una **cola de tareas**.  
**Condición de aprobación:** pasar **todos los casos de prueba** incluidos en los archivos `tpcustomtest.cc` y `tptest.cc`, sin errores de ejecución ni deadlocks.

---

## 1. Introducción

Un **thread pool** mantiene un conjunto fijo de hilos activos que pueden reutilizarse para ejecutar tareas (no se crea ni destruye un hilo por cada tarea).  
Las tareas enviadas al thread pool se encolan; cuando un hilo está disponible, toma una tarea de la cola y la ejecuta.  
Esto optimiza el rendimiento y evita la sobrecarga de crear/destruir hilos.  
Ejemplos de uso: servidores web, procesamiento por lotes, sistemas concurrentes.

---

## 2. Esquema General

1. **Inicialización:**  
   - Se crea un **dispatcher** (hilo especial para asignación de tareas) y un número fijo de **workers** (hilos de trabajo).
2. **Envío de Tareas:**  
   - Las tareas (funciones `void(void)`) se envían al thread pool mediante el método `schedule`.
3. **Cola de Tareas:**  
   - Todas las tareas se colocan en una **cola** (ej: `queue<function<void(void)>>`), que sirve como buffer entre envío y ejecución.
4. **Hilos Workers:**  
   - El **dispatcher** monitoriza la cola. Cuando hay una tarea disponible y un worker libre, asigna la tarea y despierta al worker correspondiente.
5. **Ejecución:**  
   - El worker ejecuta su tarea, y al terminar queda disponible para nuevas asignaciones.
6. **Finalización:**  
   - Los workers buscan nuevas tareas; si no hay, quedan esperando hasta ser señalizados de nuevo.

---

## 3. Especificaciones de Implementación

### Archivos relevantes

- `Semaphore.h`, `Semaphore.cc`: implementación de semáforo para sincronización.
- `thread-pool.h`, `thread-pool.cc`: archivos donde debe implementarse toda la lógica del Thread Pool.
- `main.cc`: ejemplo de uso.
- `tpcustomtest.cc`, `tptest.cc`: archivos de tests funcionales y de stress.

### Métodos de la clase `ThreadPool`

- **ThreadPool(size_t numThreads):**  
  Constructor. Inicializa el thread pool con un número fijo de hilos y lanza el dispatcher y los workers.

- **void schedule(const std::function<void(void)>& thunk):**  
  Encola el thunk (función sin parámetros, sin retorno).  
  Debe ser thread-safe (puede ser llamada desde múltiples hilos externos).  
  No debe bloquear al llamador.

- **void wait():**  
  Espera a que **todas** las tareas agendadas hasta el momento hayan terminado de ejecutarse.  
  Debe ser thread-safe y soportar múltiples llamadas.

- **~ThreadPool():**  
  Espera a que todas las tareas hayan terminado y destruye todos los recursos y hilos correctamente.

### Diseño requerido

- **Dispatcher**:  
  Hilo dedicado que:
    - Espera notificación de nuevas tareas (por ejemplo, usando un semáforo, condición, etc.).
    - Cuando hay tareas y workers libres, asigna tareas y señala a los workers.
    - Loop infinito que termina sólo al destruir el ThreadPool.

- **Workers**:  
  Cada worker ejecuta en un loop:
    - Espera señal de que tiene una tarea asignada.
    - Ejecuta la tarea.
    - Marca su disponibilidad.
    - Vuelve a esperar señal.

- **Estructura worker_t**  
  Cada worker debe almacenar:
    - thread handle
    - semáforo o condición para señalización individual
    - estado de disponibilidad
    - id (opcional)
    - almacenamiento temporal de la tarea a ejecutar

- **Sincronización**:  
  - El acceso a la cola de tareas debe estar protegido.
  - schedule y wait deben ser thread-safe.
  - No puede haber condiciones de carrera ni deadlocks.

- **Destructor**:
  - Espera a que se completen todas las tareas pendientes.
  - Da señal de terminación a dispatcher y workers para terminar los hilos limpiamente.
  - Libera todos los recursos utilizados.

- **Restricciones**:
  - No modificar la interfaz pública ni los archivos de tests.
  - No implementar el ThreadPool **sin** dispatcher (debe haber hilo dispatcher).

---

## 4. Testing

- Ejecutar los binarios generados por `tpcustomtest.cc` y `tptest.cc` para verificar la correcta implementación.
- La salida debe ser la esperada y no debe haber errores ni deadlocks.
- Usar `main.cc` para pruebas simples iniciales.

---

## 5. Consejos y Evaluación

- Seguir **exactamente** el diseño indicado.
- Priorizar claridad y legibilidad del código.
- El uso eficiente de memoria y recursos es obligatorio.
- El código debe ser robusto y cubrir condiciones límite y casos de stress.
- El docente puede requerir defensa oral para explicar la solución.

---

## Resumen de pasos clave

1. Inicializa dispatcher y workers en el constructor.
2. schedule encola funciones y notifica al dispatcher.
3. dispatcher asigna tareas a workers y los despierta.
4. workers ejecutan la tarea asignada y vuelven a esperar.
5. wait espera a que se completen todas las tareas pendientes.
6. El destructor asegura la terminación limpia de todos los hilos.

---

## Entrega

- El código debe estar listo y probado antes del **domingo 22/06/2025 23:59**.

---

**Fin del enunciado**

# Waveform Scope

VST3 que muestra la forma de onda del canal donde lo insertás, corriendo en el tiempo y coloreada por frecuencia, al estilo Rekordbox/Serato. La onda se ve en una ventana flotante, sin bordes y siempre arriba, que queda visible aunque cambies de canal en el DAW.

## Funciones

- **Modos de color**: RGB (Serato: graves rojos, medios verdes, agudos azules), 3-Band (Rekordbox: azul / naranja / blanco) y Mono.
- **Alta definición**: análisis cada 0,5 ms con pico + RMS, dibujado a resolución física (pantallas escaladas incluidas).
- **Grilla de beats** sincronizada con el transporte del host (compases marcados más fuerte), con offset en ms para compensar la latencia (por ejemplo, en el master).
- **Ventana flotante**: sin bordes, se arrastra desde cualquier parte, se redimensiona desde los bordes, recuerda posición y tamaño con el proyecto y muestra el nombre y color del track.

## Controles de la ventana

| Acción | Cómo |
|---|---|
| Mover | Arrastrar con clic izquierdo |
| Redimensionar | Arrastrar los bordes |
| Zoom | Rueda del mouse |
| Menú (modo, zoom, grilla) | Clic derecho |
| Pantalla completa | Doble clic o F11 (Esc para salir) |
| Cerrar | Ctrl + clic, o la × que aparece al pasar el mouse |

Para reabrir la ventana, usá el botón **Ventana flotante** en el panel del plugin.

## Compilar (Windows)

Requisitos: Visual Studio 2022/2026 con "Desarrollo para el escritorio con C++" y CMake 3.22 o superior (viene con Visual Studio). JUCE se descarga solo al configurar.

```bash
cmake -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target WaveformScope_VST3
```

El plugin queda en `build/WaveformScope_artefacts/Release/VST3/Waveform Scope.vst3`. Copialo a `C:\Program Files\Common Files\VST3\` y reescaneá los plugins en el DAW. También se genera una versión standalone con el target `WaveformScope_Standalone`.

## Licencia

Este proyecto se distribuye bajo la [GNU AGPLv3](LICENSE). Está hecho con [JUCE](https://juce.com), que también usa AGPLv3 para proyectos de código abierto.

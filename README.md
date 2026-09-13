# NativeChats

Overlay de chat unificado para streaming y simracing (Twitch + YouTube Live).
100% nativo en C++20 y Qt6, ultraligero y sin intermediarios en la nube.

## 📥 Descarga Rápida para Windows (sin compilar nada)

Si solo querés usar la aplicación en tu PC de streaming o simulador:

1. Ve a la sección de [Últimas Versiones](../../releases).
2. Descarga el archivo `NativeChats-Windows.zip`.
3. Descomprime la carpeta en tu Escritorio.
4. Entrá a la carpeta `NativeChats-Windows/` y ejecutá `NativeChats.exe`.

> **⚠️ Aviso para Windows 10 / 11**
>
> Al ser un proyecto de código abierto nuevo sin certificado de pago de Microsoft, Windows Defender SmartScreen puede mostrar una pantalla azul preventiva («Windows protegió su PC»).
>
> Para iniciarla, simplemente hacé clic en «Más información» y luego en «Ejecutar de todas formas». El ejecutable está verificado y libre de malware.
>
> **Importante:** no muevas `NativeChats.exe` fuera de la carpeta. Necesita los DLLs y las subcarpetas `platforms/`, `imageformats/` y `styles/` que vienen en el zip.

## Características

- **Twitch IRC nativo**: conexión directa por socket TCP, soporte de tags, emotes oficiales y `/me` (acciones).
- **YouTube Live Tracker**: extracción del chat en vivo usando el mismo endpoint interno que usa el cliente web de YouTube. **No requiere registrarse en Google Cloud, ni OAuth, ni una API key propia.**
- **Entrelazado justo (Fair Queuing)**: alterna los mensajes 1 a 1 entre Twitch y YouTube para evitar bloques de comentarios.
- **Emotes y stickers**:
  - Emotes oficiales de Twitch.
  - Catálogo global de 7TV y BetterTTV.
  - Emotes de miembro de YouTube (los que aparecen dentro del texto del mensaje).
  - Emojis Unicode a todo color.
- **Pausa de lectura (Smart Scroll-Lock)**: al subir con la rueda del ratón, el chat se congela por completo y muestra un botón flotante para regresar al fondo.
- **Historial amplio**: mantiene hasta 1000 mensajes en el log interno, y las colas de renderizado aguantan ráfagas de raids sin perder nada.
- **Panel de Ajustes en Vivo (F1)**:
  - Opacidad de fondo regulable (0% cristal → 100% opaco).
  - Cadencia de aparición regulable (0 s → 3.0 s por mensaje, con marcas cada 0.5 s).
  - Separación entre líneas y tamaño de fuente en tiempo real.
  - Cantidad de mensajes visibles en OBS (3 a 15).
  - Indicadores LED de conexión (Verde = Conectado / Rojo = Desconectado).
- **Doble modo de uso**:
  - Ventana nativa transparente y flotante para el monitor del streamer.
  - Servidor web local integrado en `http://localhost:8080` para fuentes de navegador de OBS Studio y Streamlabs.

## Guía de Uso

1. Al abrir la aplicación, presioná **F1** para desplegar el panel de ajustes.
2. Ingresá tu **canal de Twitch** (solo el nombre, ej: `boxeodeprimeraoficial`).
3. Ingresá la **URL del directo de YouTube**. Formatos aceptados:
   - `https://www.youtube.com/watch?v=VIDEO_ID` ← recomendado
   - `https://youtu.be/VIDEO_ID`
   - `https://www.youtube.com/live/VIDEO_ID`
   - `https://www.youtube.com/@canal/live` ← solo si el canal está en directo en ese momento
   - `https://www.youtube.com/shorts/VIDEO_ID`
   - **No funciona** `https://www.youtube.com/@canal/streams` (es un listado, no un directo).
4. Ajustá los deslizadores de opacidad, cadencia y tamaño de texto a tu gusto.
5. Presioná **Guardar y Aplicar** (los datos se guardan automáticamente en `config.json`).
6. Para OBS Studio o Streamlabs, agregá una **Fuente de Navegador (Browser Source)** apuntando a `http://localhost:8080`.

## Controles

| Control | Acción |
|---|---|
| `F1` | Abre / oculta el panel de ajustes |
| Arrastrar barra superior | Mueve la ventana por la pantalla |
| Arrastrar bordes / esquinas | Redimensiona la ventana |
| Rueda del ratón hacia arriba | Pausa el chat para leer mensajes pasados |
| Botón "Ir al fondo" | Reanuda el desplazamiento en vivo |
| Botón ✕ (o Alt + F4) | Cierra la aplicación limpiamente |

## Compilación para Desarrolladores

### En Linux (Fedora / RHEL)

Requisitos: compilador C++20, CMake (3.16+), Qt6 (Core, Gui, Widgets, Network) y libcurl.

```bash
# Fedora / RHEL
sudo dnf install -y gcc-c++ cmake git libcurl-devel qt6-qtbase-devel

# Compilar
git clone https://github.com/Levn1987/NativeChats.git
cd NativeChats
cmake -B build
cmake --build build -j$(nproc)
./build/NativeChats

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
```

### En Ubuntu / Debian

```bash
sudo apt install -y g++ cmake git libcurl4-openssl-dev qt6-base-dev
```

### Compilación cruzada para Windows 11 desde Fedora

**Requisitos** (todos, incluidos los imageformats que son críticos para los emotes webp de 7TV):

```bash
sudo dnf install -y \
  mingw64-gcc-c++ \
  mingw64-cmake \
  mingw64-qt6-qtbase \
  mingw64-qt6-qtimageformats \
  mingw64-qt6-qtsvg
```

**Compilar:**

```bash
cd NativeChats
mingw64-cmake -B build-win
cmake --build build-win -j$(nproc)
```

El ejecutable queda en `build-win/NativeChats.exe`.

**Importante**: el `.exe` **no corre solo**. Necesita los DLLs de Qt y MinGW, más las subcarpetas `platforms/`, `imageformats/` y `styles/`. Para armar el paquete completo:

```bash
mkdir -p dist-windows/platforms dist-windows/imageformats dist-windows/styles
cp build-win/NativeChats.exe dist-windows/

MINGW_ROOT=/usr/x86_64-w64-mingw32/sys-root/mingw

# DLLs transitivas (5 pasadas para resolver dependencias encadenadas)
for i in 1 2 3 4 5; do
  for f in dist-windows/*.dll dist-windows/*.exe; do
    [ -f "$f" ] || continue
    deps=$(x86_64-w64-mingw32-objdump -p "$f" 2>/dev/null | grep -i 'DLL Name' | awk '{print $3}')
    for dep in $deps; do
      real_dep=$(find "$MINGW_ROOT/bin" -maxdepth 1 -iname "$dep" -exec basename {} \; 2>/dev/null | head -1)
      if [ -n "$real_dep" ] && [ ! -f "dist-windows/$real_dep" ]; then
        cp "$MINGW_ROOT/bin/$real_dep" dist-windows/
      fi
    done
  done
done

# Plugins de Qt (cargados en runtime, no aparecen en objdump)
find "$MINGW_ROOT" -name "qwindows.dll" -path "*platforms*" -exec cp {} dist-windows/platforms/ \;
for p in qjpeg qgif qwebp qsvg qtiff qicns; do
  find "$MINGW_ROOT" -name "${p}.dll" -path "*imageformats*" -exec cp {} dist-windows/imageformats/ \; 2>/dev/null
done
find "$MINGW_ROOT" -name "qmodernwindowsstyle.dll" -path "*styles*" -exec cp {} dist-windows/styles/ \; 2>/dev/null

zip -r NativeChats-Windows.zip dist-windows
```

## Licencia

Este proyecto está bajo la Licencia MIT. Ver [LICENSE](LICENSE).
```

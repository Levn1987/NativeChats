# NativeChats

Overlay de chat unificado para streaming y simracing (Twitch + YouTube Live).  
100% nativo en C++20 y Qt6, ultraligero y sin intermediarios en la nube.

---

## 📥 Descarga Rápida para Windows (Sin compilar nada)

Si solo quieres usar la aplicación en tu PC de streaming o simulador:

1. Ve a la sección de **[Últimas Versiones (Releases)](https://github.com/Levn1987/NativeChats/releases)**.
2. Descarga el archivo **`NativeChats-Windows.zip`**.
3. Descomprime la carpeta en tu Escritorio y haz doble clic en **`NativeChats.exe`**.

> ℹ️ **Aviso para Windows 10 / 11:**  
> Al ser un proyecto de código abierto nuevo sin certificado de pago de Microsoft, Windows Defender SmartScreen podría mostrar una pantalla azul preventiva (*«Windows protegió su PC»*).  
> Para iniciarla, simplemente haz clic en **«Más información»** y luego en **«Ejecutar de todas formas»**. El ejecutable está verificado y libre de malware.

---

## Características

* **Twitch IRC Nativo:** Conexión directa por socket TCP con soporte de moderación (CLEARMSG y CLEARCHAT).
* **YouTube Live Tracker:** Extracción del chat en vivo sin necesidad de claves de API de Google ni OAuth.
* **Entrelazado Justo (Fair Queuing):** Alterna los mensajes 1 a 1 entre Twitch y YouTube para evitar bloques de comentarios.
* **Emotes y Stickers:**
  * Soporte de emotes oficiales de Twitch y catálogo global de 7TV y BetterTTV.
  * Soporte de stickers y emotes de miembros de YouTube.
  * Emojis Unicode a todo color en alta definición.
* **Pausa de Lectura (Smart Scroll-Lock):** Al subir con la rueda del ratón, el chat se congela por completo y muestra un botón flotante para regresar al fondo.
* **Historial Amplio:** Mantiene hasta 400 mensajes en pantalla sin borrado forzado.
* **Panel de Ajustes en Vivo (F1):**
  * Opacidad de fondo regulable (desde 0% cristal hasta 100%).
  * Cadencia de aparición regulable (de 0.2 s a 3.0 s por mensaje).
  * Separación entre líneas y tamaño de fuente en tiempo real.
  * Indicadores LED de conexión (Verde = Conectado / Rojo = Desconectado).
* **Doble Modo de Uso:**
  * Ventana nativa transparente y flotante para el monitor del streamer.
  * Servidor web local integrado en `http://localhost:8080` para fuentes de navegador de OBS Studio y Streamlabs.

---

## Guía de Uso

1. Al abrir la aplicación, presiona **F1** para desplegar el panel de ajustes.
2. Ingresa tu canal de Twitch y la URL de tu directo de YouTube.
3. Ajusta los deslizadores de opacidad, velocidad y tamaño de texto a tu gusto.
4. Presiona **Guardar y Aplicar** (los datos se guardan automáticamente en `config.json`).
5. Para OBS Studio o Streamlabs, añade una **Fuente de Navegador (Browser Source)** apuntando a `http://localhost:8080`.

---

## Controles

| Control | Acción |
| :--- | :--- |
| **F1** | Abre / Oculta el panel de ajustes. |
| **Arrastrar barra superior** | Mueve la ventana por la pantalla. |
| **Arrastrar bordes / esquinas** | Redimensiona la ventana a cualquier tamaño suavemente. |
| **Rueda del ratón hacia arriba** | Pausa el chat para leer mensajes pasados. |
| **Botón "Ir al fondo"** | Reanuda el desplazamiento en vivo. |
| **Botón ✕** (o `Alt + F4`) | Cierra la aplicación limpiamente. |

---

## Compilación para Desarrolladores

### En Linux (Fedora / RHEL / Ubuntu)

Requisitos: Compilador C++20, CMake (3.16+), Qt6 (`Core`, `Gui`, `Widgets`, `Network`) y `libcurl`.

```bash
# En Fedora / RHEL
sudo dnf install -y gcc-c++ cmake git libcurl-devel qt6-qtbase-devel

# Compilar
git clone https://github.com/Levn1987/NativeChats.git
cd NativeChats
cmake -B build
cmake --build build -j$(nproc)
./build/NativeChats
```

### Compilación cruzada para Windows 11 desde Fedora

```bash
sudo dnf install -y mingw64-gcc-c++ mingw64-cmake mingw64-qt6-qtbase

cd NativeChats
mingw64-cmake -B build-win
cmake --build build-win -j$(nproc)
```

El ejecutable generado estará en `build-win/NativeChats.exe`.

---

## Licencia

Este proyecto está bajo la Licencia **MIT**.

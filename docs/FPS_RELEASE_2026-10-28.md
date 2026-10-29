# Entrega local de rendimiento — 2026-10-28

Target: Zombie Shooter Free 3.6.1 / 1161, ARMv7 SoftFP.
SO original: 9,908,972 bytes; SHA256
`cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1`.
Verificado desde el XAPK suministrado; las copias antiguas de demo/data no se reemplazaron.

VPK: `artifacts/fps-2026-10-28/zombie_shooter-release.vpk`.
SHA256: `1415a032653b2f1df172c6e315e86b745f87cb26706d711bf3bbce44e384dfe5`.
ELF sin strip: `artifacts/fps-2026-10-28/so_loader-release.elf`.
SHA256: `99df10c9df552768611bcfa4a0aba58a5b937230b5495f2d5852b2d5f76ecf4a`.

Cambios:

- El informe periódico de eglSwapBuffers usaba logger_force_sync incluso con
  log_mode 1. Ahora usa logger_flush y el informe sólo se calcula con log_mode >=2.
  Fatal y sincronizaciones explícitas conservan su contrato durable.
- Lecturas/seek/poll omiten lookup de hilo, temporizadores y contadores con
  log_mode 0/1. Conservan backend, argumentos, resultados, errno, normalización
  de rutas, índice negativo y lecturas bufferizadas.
- El detector de framebuffer todavía usaba el retorno de 3.5.3, 0x46BF59.
  La biblioteca exacta 3.6.1 llama glTexSubImage2D en 0x47EEC8 y retorna a
  0x47EECD. Al no reconocer esa subida, upload_guard nunca se habilitaba y
  GRAPH::softwareTact tampoco podía reutilizar cuadros. Se corrige con guardas
  de símbolo/entrada e instrucciones completas de preparación de argumentos.
  La política adaptativa y sus invalidaciones existentes se mantienen.
- software_width admite 768 (~768x435), sin cambiar DisplayMetrics ni salida
  física 960x544. Predeterminado: 864. No se cambian coordenadas de controles.

Archivos de implementación: source/utils/{glutil.c,perf.c,logger.c,logger.h,
settings.c,settings.h,render_scale.c,render_scale_policy.h,
software_framebuffer_contract.h}. Se ejecutaron los preparadores existentes
de 3.6.1/direct Registry/shaders necesarios para reproducir el perfil del CI.
Los cambios locales previos se conservaron; su diff inicial está en
artifacts/fps-2026-10-28/preexisting.patch. Sin ramas, commits ni publicación.

Evidencia: SO exacta + desensamblado/relocación PLT de UnLock; estado de ingeniería
2026-09-29; informes físicos de fsync y render/upload. No se reutilizan cifras
de Debug verbose como benchmark. No se activan experimentos VitaGL ni RGB565.

Verificación: STATICALLY VERIFIED / BUILD VERIFIED / VPK VERIFIED.
Release O3/g1, SDK /usr/local/vitasdk SoftFP, VitaGL baseline. No nueva build Debug.
Pasaron O0/O3: contrato exacto ELF/PLT/guardas de engine-música-escalado,
rechazo de instrucciones alteradas, reutilización/invalidez, logger real,
wrappers I/O reales, escalado/settings, preservación de texturas, caché de
shaders, JNI, controles/DisplayMetrics/Registry y asset index.
REAL VITA VERIFIED / GAMEPLAY VERIFIED de esta entrega: PENDIENTE.
No se afirma mejora de FPS ni tiempos de carga medidos.

Prueba: editar únicamente estas líneas de ux0:data/zombieshooter/config.txt;
conservar los demás ajustes. Reiniciar el juego tras cada cambio.

| Recorrido | software_width | software_frameskip | log_mode |
|---|---:|---:|---:|
| A, control sin reutilización | 864 | 0 | 2 |
| B, reutilización adaptativa | 864 | 1 | 2 |
| C, resolución experimental | 768 | 1 | 2 |

En los tres: lanzamiento hasta menú, menú a partida, misma escena 60–90 s con
disparos/explosiones/linterna, pausa/opciones, cambio de zona, retorno al menú,
cierre y relanzamiento para comprobar progreso. Comparar audio, iluminación,
transparencias, HUD, puntería y controles. En B/C buscar una vez
software_upload_contract valid=1 y aumentos de skipped_framebuffer_uploads_lifetime;
rates distingue presents, ticks e imágenes nuevas. Enviar esos tres logs y
observaciones; si hay crash, sustituir uno por el .psp2dmp correspondiente.
Para uso normal: log_mode 1. Para desactivar reutilización: software_frameskip 0.
Para desactivar reducción experimental: software_width 864.

Caché persistente de texturas: NO implementada en esta entrega temprana.
PNGLoad(char const*,char**,unsigned*,unsigned*) en 0x48DAC1 es un candidato
con identidad de recurso, pero usa el VFS del motor y sus resultados alimentan
también VID_SOFTWARE_PNG. glTexImage2D carece de identidad de archivo y no prueba
inmutabilidad; reducir allí no permite validar atlas, dimensiones en píxeles,
actualizaciones futuras o render targets. La siguiente tarea prioritaria es
validar identidad VFS/archivo fuente y propiedad/formato de salida de PNGLoad,
empezando por caché sin pérdida de recursos inmutables. No se introducen una
caché ficticia, transformaciones sin validar ni conversiones al arranque.
Comparación caché vacía/existente: no aplicable todavía.

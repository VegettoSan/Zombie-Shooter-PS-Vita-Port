# Prueba física — pase de ingeniería 2026-09-28

Instalar uno de los VPK entregados con el mismo title ID ZOMB00001. No sustituir
SO, assets, música ni datos de campaña por fixtures de tests. Respaldar los
archivos de progreso existentes antes de experimentar; no borrar saves.
Usar el ELF de esa variante para cualquier dump. Reiniciar completamente
el proceso tras cambiar config; conservar controles.txt actual para la primera
prueba y registrar si tiene remapping. No comparar Debug verbose con Release.

La entrega trae presets completos y una copia `VALIDACION.json` con hashes.
El preset `Release-baseline` conserva lo reportado por el usuario:
log_mode 0, cache16, width864, framebuffer_5650, frameskip1, diagnostics0.
Para recoger métricas en Release cambiar sólo log_mode a 2; registrar también
un tramo log0, porque logging tiene coste.

| Prueba | Variante / cambios aislados | Qué comprobar |
|---|---|---|
| S1 | Debug, log2, frameskip1, diag0 | Pasar tutorial, avanzar/checkpoint, revisar SAVE put/encrypt/commit y main/bak. No basta que aparezca un archivo |
| S2 | Cerrar por completo y relanzar la misma Debug | Leer store, continuar estado correcto y confirmar descifrado/validación; 2 ciclos de cierre. Volver al tutorial = sigue fallando |
| I1 | Misma Debug, strict/control file identificado | Pulsar y soltar por separado Cross/Circle/Square/Triangle/L/R/D-pad/rear-left/right, en menú y gameplay; relacionar physical/emitted/accepted/Binder y acción observada |
| F0 | Misma escena/objeto, Release PERF, skip1/5650/diag0 | Capturar flicker, textura afectada, presents y new software frames; conservar distancia/cámara/luces |
| F1 | Sólo frameskip0 | Si desaparece, localizar interacción de productor/reuse; no atribuir todavía a COW |
| F2 | Restaurar skip1, sólo diag4 | Raster reuse sigue activo, upload skip deshabilitado; separa las dos decisiones |
| F3 | Restaurar diag0, sólo diag2 | COW vuelve a copia completa con misma asignación/free diferido |
| F4 | Restaurar diag0, sólo diag1 | GPU finish antes de updates; discriminador lento de ownership. No usar para FPS normal |
| F5 | Restaurar diag0, sólo framebuffer_5652 | Diagnóstico corregido, más lento. El valor1 equivale a0 y no sirve como comparación |
| L1 | Release PERF, baseline; misma escena y cámara | 15s OFF, 15s flashlight ON, 15s OFF; anotar segundos exactos. Repetir con igual grupo de luces; separar explosiones de linterna |
| P1 | Release, mismo tramo, log0 y log2 por separado | Sonido/SFX, alpha/gamma, estabilidad; presents y software frames reales, no sólo tasa de presentación |
| C0/C1 | Release PERF, sólo negative cache0/1 | Arranque con proceso frío; mismo estado/cache shader. 3 repeticiones alternadas; cronómetro a menú y a gameplay, avoided_opens, bytes y errores. No borrar todo el cache simultáneamente |

Target input: sticks move/aim; A Select/Continue, B Back, Y Buy ammo,
LB next weapon, RB previous, LT medkit, RT grenade; D-pad up medkit/down grenade/
left previous/right next. X no tiene target gameplay inventado. Una pulsación
no debe producir dos acciones ajenas. Si fallan teclas ya aceptadas por el
motor, la traza dirige el siguiente pase hacia bindings/scripts.

Debug log2 activa trazas compactas sin el spam de log3. Reads tienen presupuesto
separado para que startup no agote los puts; los hooks se instalan únicamente
si coinciden símbolo, offset y prólogo. Si aparece guard rejected, no interpretar
la ausencia de una traza como falta de actividad. Los logs anteriores son de
otra build: buscar el build_id nuevo del manifest.

Para la primera devolución bastan como máximo tres archivos:

1. Log completo de S1/I1 (progreso y botones, build ID visible).
2. Log completo de S2 (relaunch que conserva progreso o vuelve al tutorial).
3. Log de la comparación F0/F1 más informativa o, si hubo crash, un .psp2dmp
   junto con el nombre exacto del ELF usado. Informar objeto/escena y opción.

Los otros A/B pueden devolver sus resultados en una tabla de texto al principio.
No marcar REAL VITA VERIFIED/GAMEPLAY VERIFIED hasta completar estos criterios.

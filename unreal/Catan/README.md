# Catan UE5 vertical slice

Проект рассчитан на Unreal Engine 5.8 и использует существующий C++20-движок
из `../../src` как единственный источник игровых правил.

## Что уже работает

- процедурное поле из 19 гексов, 54 кликабельных узлов и 72 рёбер;
- случайная раскладка ресурсов, чисел и разбойника из `GameController`;
- локальная партия на двух игроков и начальная расстановка кликами;
- окраска построек по игрокам и вывод ошибок правил на экран;
- Common UI HUD с фазой, кубиками, ресурсами, очками и остатком фигур;
- кнопки броска кубиков, покупки карты, завершения хода и выбора постройки;
- RTS-камера: `WASD` — перемещение, `Q/E` — вращение, колесо — масштаб;
- Blueprint-friendly снимок состояния (`FCatanGameView`) без передачи в UI
  указателей из чистого C++ core.

Графика пока намеренно прототипная: стандартные меши и материалы Engine,
без внешних ассетов и UI-панелей.

## Запуск

Откройте `Catan.uproject` через Unreal Editor 5.8 и нажмите Play. Создавать
уровень или Blueprint вручную не требуется: `ACatanGameMode` сам создаёт поле,
камеру и контроллер. На стартовой фазе нажмите свободный перекрёсток, затем
соседнее ребро; после каждого допустимого действия цвет фигуры и текущая фаза
обновятся.

Сборка из терминала на macOS:

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" \
  CatanEditor Mac Development \
  -Project="$PWD/unreal/Catan/Catan.uproject" -WaitMutex -NoHotReload
```

Headless smoke-test:

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/unreal/Catan/Catan.uproject" -game -nullrhi -unattended -nosplash \
  -ExecCmds="quit"
```

## Граница core / Unreal

`UCatanGameSubsystem` владеет `GameController`. UI и поле получают копируемый
`FCatanGameView` и посылают команды через методы `Try...` subsystem.
Поэтому будущая сетевая версия сможет оставить `GameController` на
authoritative server, а клиентам реплицировать только снимок и результаты
команд.

`CatanCoreCompilation.cpp` подключает исходники core в UE-модуль одним
translation unit. Для модуля включены C++ exceptions, потому что публичный API
движка сообщает о нарушениях правил исключениями. RTTI не требуется: тип
постройки определяется виртуальным `Building::canUpgrade()`.

Следующий практический этап — отдельные диалоги для сброса ресурсов, выбора
жертвы разбойника, торговли и применения карт развития. После завершения
локального игрового цикла можно переносить команды на authoritative server.

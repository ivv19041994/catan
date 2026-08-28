#include "CatanTextResources.h"

namespace
{
class FCatanEnglishTextResources final : public ICatanTextResources
{
public:
    FString Get(const FString& Key) const override
    {
        if (const FString* Value = SemanticValues.Find(Key)) return *Value;
        return Key;
    }
    bool Has(const FString&) const override { return true; }

private:
    const TMap<FString, FString> SemanticValues = {
        {TEXT("RESOURCE WOOD"), TEXT("Wood")}, {TEXT("RESOURCE CLAY"), TEXT("Clay")},
        {TEXT("RESOURCE HAY"), TEXT("Hay")}, {TEXT("RESOURCE SHEEP"), TEXT("Sheep")},
        {TEXT("RESOURCE STONE"), TEXT("Stone")},
        {TEXT("PHASE ROLL DICE"), TEXT("Roll dice")},
        {TEXT("PHASE DISCARD RESOURCES"), TEXT("Discard resources")},
        {TEXT("CHOOSE STEAL VICTIM"), TEXT("CHOOSE A PLAYER TO STEAL FROM")},
        {TEXT("PLAYER LABEL"), TEXT("Player")},
        {TEXT("PLAYER NAME HINT"), TEXT("Player name")},
        {TEXT("ROAD BUILDING CARDS"), TEXT("Roads")},
        {TEXT("MONOPOLY CARD"), TEXT("Monopoly")},
        {TEXT("missing road"), TEXT("road")},
        {TEXT("missing settlement"), TEXT("settlement")},
        {TEXT("missing city"), TEXT("city")},
        {TEXT("missing development card"), TEXT("development card")},
        {TEXT("final resource cards"), TEXT("resource cards")},
        {TEXT("remaining roads"), TEXT("roads")},
        {TEXT("event one bot"), TEXT("bot")},
        {TEXT("event multiple bots"), TEXT("bots")},
        {TEXT("event resource cards"), TEXT("resource cards")}
    };
};

class FCatanRussianTextResources final : public ICatanTextResources
{
public:
    FString Get(const FString& Key) const override
    {
        if (const FString* Value = Values.Find(Key)) return *Value;
        return Key;
    }
    bool Has(const FString& Key) const override { return Values.Contains(Key); }

private:
    const TMap<FString, FString> Values = {
        {TEXT("Starting..."), TEXT("Запуск...")},
        {TEXT("SHOW EVENTS & HELP"), TEXT("СОБЫТИЯ И ПОМОЩЬ")},
        {TEXT("HIDE EVENTS & HELP"), TEXT("СКРЫТЬ ПОДРОБНОСТИ")},
        {TEXT("EVENTS"), TEXT("СОБЫТИЯ")},
        {TEXT("YOUR HAND"), TEXT("ВАША РУКА")},
        {TEXT("WOOD"), TEXT("ДЕРЕВО")}, {TEXT("CLAY"), TEXT("ГЛИНА")},
        {TEXT("HAY"), TEXT("ЗЕРНО")}, {TEXT("SHEEP"), TEXT("ОВЦЫ")},
        {TEXT("ORE"), TEXT("РУДА")},
        {TEXT("RESOURCE WOOD"), TEXT("Дерево")}, {TEXT("RESOURCE CLAY"), TEXT("Глина")},
        {TEXT("RESOURCE HAY"), TEXT("Зерно")}, {TEXT("RESOURCE SHEEP"), TEXT("Овцы")},
        {TEXT("RESOURCE STONE"), TEXT("Руда")},
        {TEXT("SHOW PLAYERS & COSTS"), TEXT("ИГРОКИ И ЦЕНЫ")},
        {TEXT("HIDE PLAYERS & COSTS"), TEXT("СКРЫТЬ ПОДРОБНОСТИ")},
        {TEXT("Development: 0"), TEXT("Карты развития: 0")},
        {TEXT("PLAYERS"), TEXT("ИГРОКИ")}, {TEXT("BUILD COSTS"), TEXT("СТОИМОСТЬ СТРОИТЕЛЬСТВА")},
        {TEXT("SAVED GAMES"), TEXT("СОХРАНЁННЫЕ ИГРЫ")},
        {TEXT("No saved games"), TEXT("Нет сохранённых игр")},
        {TEXT("Costs appear for the current player"), TEXT("Здесь появится стоимость для текущего игрока")},
        {TEXT("ACTIONS"), TEXT("ДЕЙСТВИЯ")}, {TEXT("ROLL DICE"), TEXT("БРОСИТЬ КУБИКИ")},
        {TEXT("ROLL"), TEXT("БРОСОК")}, {TEXT("SETTLEMENT"), TEXT("ПОСЕЛЕНИЕ")},
        {TEXT("SETTLE"), TEXT("ПОСЕЛЕНИЕ")}, {TEXT("ROAD"), TEXT("ДОРОГА")},
        {TEXT("ARMY"), TEXT("АРМИЯ")},
        {TEXT("CITY"), TEXT("ЗАМОК")}, {TEXT("BUY DEV"), TEXT("КУПИТЬ КАРТУ")},
        {TEXT("USE DEV"), TEXT("ИГРАТЬ КАРТУ")}, {TEXT("VIEW DEV"), TEXT("КАРТЫ")},
        {TEXT("DEV"), TEXT("КАРТЫ")}, {TEXT("TRADE"), TEXT("ТОРГОВЛЯ")},
        {TEXT("END TURN"), TEXT("ЗАВЕРШИТЬ ХОД")}, {TEXT("END"), TEXT("ЗАВЕРШИТЬ")},
        {TEXT("DISCARD RESOURCES"), TEXT("СБРОС РЕСУРСОВ")},
        {TEXT("CONFIRM DISCARD"), TEXT("ПОДТВЕРДИТЬ СБРОС")},
        {TEXT("CHOOSE STEAL VICTIM"), TEXT("ВЫБЕРИТЕ ИГРОКА ДЛЯ КРАЖИ")},
        {TEXT("PLAYER LABEL"), TEXT("Игрок")}, {TEXT("DEVELOPMENT CARDS"), TEXT("КАРТЫ РАЗВИТИЯ")},
        {TEXT("PLAY KNIGHT"), TEXT("СЫГРАТЬ РЫЦАРЯ")},
        {TEXT("PLAY ROAD BUILDING"), TEXT("СЫГРАТЬ СТРОИТЕЛЬСТВО ДОРОГ")},
        {TEXT("Resources for Year of Plenty / Monopoly"), TEXT("Ресурсы для Изобилия / Монополии")},
        {TEXT("PLAY YEAR OF PLENTY"), TEXT("СЫГРАТЬ ИЗОБИЛИЕ")},
        {TEXT("PLAY MONOPOLY"), TEXT("СЫГРАТЬ МОНОПОЛИЮ")},
        {TEXT("PLAY MONOPOLY (FIRST RESOURCE)"), TEXT("СЫГРАТЬ МОНОПОЛИЮ (ПЕРВЫЙ РЕСУРС)")},
        {TEXT("YEAR OF PLENTY"), TEXT("ИЗОБИЛИЕ")},
        {TEXT("MONOPOLY"), TEXT("МОНОПОЛИЯ")},
        {TEXT("Choose exactly two resources"), TEXT("Выберите ровно два ресурса")},
        {TEXT("Choose one resource from every opponent"), TEXT("Выберите один ресурс у всех соперников")},
        {TEXT("CLOSE"), TEXT("ЗАКРЫТЬ")}, {TEXT("BANK"), TEXT("БАНК")},
        {TEXT("OTHER PLAYER"), TEXT("ДРУГОЙ ИГРОК")},
        {TEXT("Choose one resource to give and one to receive"), TEXT("Выберите отдаваемый и получаемый ресурс")},
        {TEXT("YOU GIVE"), TEXT("ВЫ ОТДАЁТЕ")}, {TEXT("YOU RECEIVE"), TEXT("ВЫ ПОЛУЧАЕТЕ")},
        {TEXT("CONFIRM BANK TRADE"), TEXT("ПОДТВЕРДИТЬ ОБМЕН С БАНКОМ")},
        {TEXT("OFFER TO"), TEXT("ПРЕДЛОЖИТЬ ИГРОКУ")}, {TEXT("SEND OFFER"), TEXT("ОТПРАВИТЬ ПРЕДЛОЖЕНИЕ")},
        {TEXT("ACTIVE PLAYER TRADE"), TEXT("АКТИВНОЕ ПРЕДЛОЖЕНИЕ")},
        {TEXT("ACCEPT OFFER"), TEXT("ПРИНЯТЬ ПРЕДЛОЖЕНИЕ")},
        {TEXT("DECLINE OFFER"), TEXT("ОТКЛОНИТЬ ПРЕДЛОЖЕНИЕ")},
        {TEXT("WITHDRAW OFFER"), TEXT("ОТОЗВАТЬ ПРЕДЛОЖЕНИЕ")},
        {TEXT("NOT ENOUGH RESOURCES"), TEXT("НЕДОСТАТОЧНО РЕСУРСОВ")},
        {TEXT("GAME OVER"), TEXT("ИГРА ОКОНЧЕНА")}, {TEXT("NEW GAME"), TEXT("НОВАЯ ИГРА")},
        {TEXT("EXIT"), TEXT("ВЫХОД")}, {TEXT("Choose a game mode"), TEXT("Выберите режим игры")},
        {TEXT("PLAYER"), TEXT("ИГРОК")}, {TEXT("ONLINE"), TEXT("СЕТЕВАЯ ИГРА")},
        {TEXT("PLAY AGAINST BOTS"), TEXT("ИГРА ПРОТИВ БОТОВ")},
        {TEXT("SETTINGS"), TEXT("НАСТРОЙКИ")}, {TEXT("Choose how to connect"), TEXT("Выберите способ подключения")},
        {TEXT("EFFECTS VOLUME"), TEXT("ГРОМКОСТЬ ЭФФЕКТОВ")},
        {TEXT("MUSIC VOLUME"), TEXT("ГРОМКОСТЬ МУЗЫКИ")},
        {TEXT("HAPTIC FEEDBACK"), TEXT("ВИБРООТКЛИК")},
        {TEXT("COLOR ACCESSIBILITY"), TEXT("ЦВЕТОВАЯ ДОСТУПНОСТЬ")},
        {TEXT("ON"), TEXT("ВКЛ")}, {TEXT("OFF"), TEXT("ВЫКЛ")},
        {TEXT("STANDARD"), TEXT("СТАНДАРТНАЯ")},
        {TEXT("HIGH CONTRAST"), TEXT("ВЫСОКИЙ КОНТРАСТ")},
        {TEXT("DEUTERANOPIA"), TEXT("ДЕЙТЕРАНОПИЯ")},
        {TEXT("PROTANOPIA"), TEXT("ПРОТАНОПИЯ")},
        {TEXT("TRITANOPIA"), TEXT("ТРИТАНОПИЯ")},
        {TEXT("Resource names remain visible in every color mode."),
            TEXT("Названия ресурсов видны при любом цветовом режиме.")},
        {TEXT("LOCAL NETWORK"), TEXT("ЛОКАЛЬНАЯ СЕТЬ")},
        {TEXT("DEDICATED SERVER"), TEXT("ВЫДЕЛЕННЫЙ СЕРВЕР")}, {TEXT("BACK"), TEXT("НАЗАД")},
        {TEXT("Host a LAN lobby, find one automatically, or enter its address."), TEXT("Создайте LAN-лобби, найдите его автоматически или введите адрес.")},
        {TEXT("Lobby name"), TEXT("Название лобби")}, {TEXT("HOST ONLINE (LAN)"), TEXT("СОЗДАТЬ LAN-ЛОББИ")},
        {TEXT("LOAD SAVED LAN GAME"), TEXT("ЗАГРУЗИТЬ СОХРАНЁННУЮ LAN-ИГРУ")},
        {TEXT("No search results yet"), TEXT("Поиск ещё не выполнялся")},
        {TEXT("No LAN lobbies found"), TEXT("LAN-лобби не найдены")},
        {TEXT("REFRESH LAN LOBBIES"), TEXT("ОБНОВИТЬ СПИСОК LAN-ЛОББИ")},
        {TEXT("JOIN SELECTED"), TEXT("ПРИСОЕДИНИТЬСЯ К ВЫБРАННОМУ")},
        {TEXT("Host address, e.g. 192.168.1.20:7777"), TEXT("Адрес хоста, например 192.168.1.20:7777")},
        {TEXT("JOIN BY ADDRESS"), TEXT("ВОЙТИ ПО АДРЕСУ")}, {TEXT("LAN ready"), TEXT("LAN готова")},
        {TEXT("Create a new game or join an existing lobby by token."), TEXT("Создайте игру или войдите в существующее лобби по токену.")},
        {TEXT("Server IP, e.g. 192.168.1.20:17777"), TEXT("IP сервера, например 192.168.1.20:17777")},
        {TEXT("CREATE GAME ON SERVER"), TEXT("СОЗДАТЬ ИГРУ НА СЕРВЕРЕ")},
        {TEXT("Lobby token, e.g. ABCD-EFGH"), TEXT("Токен лобби, например ABCD-EFGH")},
        {TEXT("JOIN GAME BY LOBBY TOKEN"), TEXT("ВОЙТИ В ИГРУ ПО ТОКЕНУ")},
        {TEXT("Server ready"), TEXT("Сервер готов")},
        {TEXT("Choose the total number of players."), TEXT("Выберите общее число игроков.")},
        {TEXT("2 players"), TEXT("2 игрока")}, {TEXT("3 players"), TEXT("3 игрока")},
        {TEXT("4 players"), TEXT("4 игрока")}, {TEXT("START BOT GAME"), TEXT("НАЧАТЬ ИГРУ С БОТАМИ")},
        {TEXT("CONFIRM ACTION"), TEXT("ПОДТВЕРЖДЕНИЕ")}, {TEXT("CONFIRM"), TEXT("ПОДТВЕРДИТЬ")},
        {TEXT("CANCEL"), TEXT("ОТМЕНА")}, {TEXT("GAME LOBBY"), TEXT("ИГРОВОЕ ЛОББИ")},
        {TEXT("Host address"), TEXT("Адрес хоста")}, {TEXT("Waiting for players..."), TEXT("Ожидание игроков...")},
        {TEXT("READY"), TEXT("ГОТОВ")}, {TEXT("NOT READY"), TEXT("НЕ ГОТОВ")},
        {TEXT("START GAME"), TEXT("НАЧАТЬ ИГРУ")}, {TEXT("COPY LOBBY TOKEN"), TEXT("КОПИРОВАТЬ ТОКЕН ЛОББИ")},
        {TEXT("LEAVE LOBBY"), TEXT("ПОКИНУТЬ ЛОББИ")},
        {TEXT("Expected players:"), TEXT("Ожидаемые игроки:")}, {TEXT("WAITING"), TEXT("ОЖИДАНИЕ")},
        {TEXT("The restored game starts after every expected name reconnects and is ready."),
            TEXT("Сохранённая игра начнётся, когда все ожидаемые игроки войдут и будут готовы.")},
        {TEXT("PLAYER NAME"), TEXT("ИМЯ")}, {TEXT("LANGUAGE"), TEXT("ЯЗЫК")},
        {TEXT("SAVE SETTINGS"), TEXT("СОХРАНИТЬ НАСТРОЙКИ")},
        {TEXT("PLAYER NAME HINT"), TEXT("Имя игрока")}, {TEXT("English"), TEXT("English")},
        {TEXT("Russian"), TEXT("Русский")}, {TEXT("Settings saved"), TEXT("Настройки сохранены")},
        {TEXT("New local game started"), TEXT("Новая локальная игра запущена")},
        {TEXT("Choose a player to steal from"), TEXT("Выберите игрока, у которого нужно украсть ресурс")},
        {TEXT("Select a target on the board"), TEXT("Выберите цель на игровом поле")},
        {TEXT("Click a free intersection to place a settlement"), TEXT("Нажмите на свободный перекрёсток, чтобы построить поселение")},
        {TEXT("Click an adjacent edge to place a road"), TEXT("Нажмите на соседнее ребро, чтобы построить дорогу")},
        {TEXT("Roll both dice to start the turn"), TEXT("Бросьте оба кубика, чтобы начать ход")},
        {TEXT("Choose an action, then click its target on the board"), TEXT("Выберите действие, затем его цель на поле")},
        {TEXT("Choose exactly half of the shown player's resources to discard"), TEXT("Выберите ровно половину ресурсов указанного игрока для сброса")},
        {TEXT("Click a different hex to move the robber"), TEXT("Выберите другой гекс для перемещения разбойника")},
        {TEXT("Click up to two valid road edges"), TEXT("Выберите до двух доступных рёбер для дорог")},
        {TEXT("Game finished"), TEXT("Игра окончена")},
        {TEXT("Setup: place settlement"), TEXT("Расстановка: поселение")},
        {TEXT("Setup: place road"), TEXT("Расстановка: дорога")},
        {TEXT("PHASE ROLL DICE"), TEXT("Бросок кубиков")}, {TEXT("Build and trade"), TEXT("Строительство и торговля")},
        {TEXT("PHASE DISCARD RESOURCES"), TEXT("Сброс ресурсов")}, {TEXT("Move robber"), TEXT("Перемещение разбойника")},
        {TEXT("Road Building card"), TEXT("Карта строительства дорог")},
        {TEXT("Finished"), TEXT("Завершено")}, {TEXT("Unknown phase"), TEXT("Неизвестная фаза")},
        {TEXT("Current:"), TEXT("Сейчас ходит:")}, {TEXT("Dice:"), TEXT("Кубики:")},
        {TEXT("Waiting for"), TEXT("Ожидание игрока")},
        {TEXT("RESOURCE CARDS"), TEXT("КАРТ РЕСУРСОВ")},
        {TEXT("Knight"), TEXT("Рыцарь")}, {TEXT("ROAD BUILDING CARDS"), TEXT("Дороги")},
        {TEXT("Plenty"), TEXT("Изобилие")}, {TEXT("MONOPOLY CARD"), TEXT("Монополия")},
        {TEXT("ready next turn"), TEXT("будут доступны в следующий ход")},
        {TEXT("YOUR RESOURCES:"), TEXT("ВАШИ РЕСУРСЫ:")},
        {TEXT("LONGEST ROAD"), TEXT("САМАЯ ДЛИННАЯ ДОРОГА")},
        {TEXT("LARGEST ARMY"), TEXT("САМАЯ БОЛЬШАЯ АРМИЯ")},
        {TEXT("YOU"), TEXT("ВЫ")}, {TEXT("BOT"), TEXT("БОТ")},
        {TEXT("Pieces:"), TEXT("Фигуры:")}, {TEXT("settlements"), TEXT("поселений")},
        {TEXT("cities"), TEXT("замков")}, {TEXT("remaining roads"), TEXT("дорог")},
        {TEXT("DEV CARD"), TEXT("КАРТА РАЗВИТИЯ")},
        {TEXT("DISCARD"), TEXT("СБРОСИТЬ")}, {TEXT("RESOURCES"), TEXT("РЕСУРСОВ")},
        {TEXT("Ready to play:"), TEXT("Готово к использованию:")},
        {TEXT("Bought this turn:"), TEXT("Куплено в этот ход:")},
        {TEXT("available next turn."), TEXT("доступно в следующий ход.")},
        {TEXT("Victory point cards:"), TEXT("Карты победных очков:")},
        {TEXT("passive, they are never played."), TEXT("пассивные, их не нужно разыгрывать.")},
        {TEXT("Buy a random development card?"), TEXT("Купить случайную карту развития?")},
        {TEXT("This costs 1 hay, 1 sheep and 1 ore."), TEXT("Стоимость: 1 зерно, 1 овца и 1 руда.")},
        {TEXT("Build this road?"), TEXT("Построить эту дорогу?")},
        {TEXT("Build this settlement?"), TEXT("Построить это поселение?")},
        {TEXT("Upgrade this settlement to a city?"), TEXT("Улучшить это поселение до замка?")},
        {TEXT("The selected target is highlighted in red."), TEXT("Выбранная цель подсвечена красным.")},
        {TEXT("Roll the dice before building or trading."), TEXT("Перед строительством или торговлей бросьте кубики.")},
        {TEXT("All purchases are affordable. Choose an action."), TEXT("Доступны все покупки. Выберите действие.")},
        {TEXT("Need more resources for:"), TEXT("Нужно больше ресурсов для:")},
        {TEXT("missing road"), TEXT("дороги")},
        {TEXT("missing settlement"), TEXT("поселения")},
        {TEXT("missing city"), TEXT("замка")},
        {TEXT("missing development card"), TEXT("карты развития")},
        {TEXT("You have no development cards available to play."), TEXT("У вас нет карт развития, доступных для использования.")},
        {TEXT("Build a settlement: wood + clay + hay + sheep"), TEXT("Поселение: дерево + глина + зерно + овца")},
        {TEXT("Build a road: wood + clay"), TEXT("Дорога: дерево + глина")},
        {TEXT("Upgrade your settlement to a city: 2 hay + 3 ore"), TEXT("Улучшить поселение до замка: 2 зерна + 3 руды")},
        {TEXT("Buy a development card: hay + sheep + ore"), TEXT("Карта развития: зерно + овца + руда")},
        {TEXT("Move the robber and steal from an adjacent player"), TEXT("Переместить разбойника и украсть ресурс у соседнего игрока")},
        {TEXT("Place two roads for free"), TEXT("Бесплатно построить две дороги")},
        {TEXT("Take the two selected resources"), TEXT("Получить два выбранных ресурса")},
        {TEXT("Take the selected resource from every opponent"), TEXT("Забрать выбранный ресурс у всех соперников")},
        {TEXT("CATAN"), TEXT("КОЛОНИЗАТОРЫ")},
        {TEXT("HOW TO PLAY"), TEXT("КАК ИГРАТЬ")},
        {TEXT("WELCOME TO CATAN"), TEXT("ДОБРО ПОЖАЛОВАТЬ")},
        {TEXT("Build, trade and race to 10 victory points."),
            TEXT("Стройте, торгуйте и первым наберите 10 победных очков.")},
        {TEXT("Set your name and language before you begin."),
            TEXT("Перед началом выберите имя и язык.")},
        {TEXT("TOUCH CONTROLS"), TEXT("УПРАВЛЕНИЕ")},
        {TEXT("Drag with one finger to move the camera."),
            TEXT("Перемещайте камеру одним пальцем.")},
        {TEXT("Pinch to zoom. Tap highlighted intersections, roads and hexes."),
            TEXT("Сводите пальцы для масштаба. Нажимайте подсвеченные перекрёстки, дороги и гексы.")},
        {TEXT("Use the two side buttons to open help, players and build costs."),
            TEXT("Боковые кнопки открывают помощь, список игроков и стоимость строительства.")},
        {TEXT("YOUR TURN"), TEXT("ВАШ ХОД")},
        {TEXT("Follow the phase hint in the top-left corner."),
            TEXT("Следуйте подсказке фазы в левом верхнем углу.")},
        {TEXT("Roll first, then build, trade or play a development card."),
            TEXT("Сначала бросьте кубики, затем стройте, торгуйте или играйте карту развития.")},
        {TEXT("End your turn when you are done. Your own resources are always visible."),
            TEXT("Завершите ход, когда закончите. Ваши ресурсы видны всегда.")},
        {TEXT("NEXT"), TEXT("ДАЛЕЕ")}, {TEXT("START PLAYING"), TEXT("НАЧАТЬ ИГРАТЬ")},
        {TEXT("SKIP"), TEXT("ПРОПУСТИТЬ")}, {TEXT("STEP"), TEXT("ШАГ")},
        {TEXT("OF"), TEXT("ИЗ")},
        {TEXT("STEP 1 OF 3"), TEXT("ШАГ 1 ИЗ 3")},
        {TEXT("STEP 2 OF 3"), TEXT("ШАГ 2 ИЗ 3")},
        {TEXT("STEP 3 OF 3"), TEXT("ШАГ 3 ИЗ 3")},
        {TEXT("RECONNECT AS"), TEXT("ПЕРЕПОДКЛЮЧИТЬСЯ КАК")},
        {TEXT("RECONNECT TO SAVED GAME"), TEXT("ВЕРНУТЬСЯ В СОХРАНЁННУЮ ИГРУ")},
        {TEXT("HOST"), TEXT("ХОСТ")},
        {TEXT("2–4 players; everyone must be ready."),
            TEXT("2–4 игрока; все должны быть готовы.")},
        {TEXT("Server:"), TEXT("Сервер:")}, {TEXT("Lobby token:"), TEXT("Токен лобби:")},
        {TEXT("Your private player token:"), TEXT("Ваш личный токен игрока:")},
        {TEXT("Share this address:"), TEXT("Поделитесь этим адресом:")},
        {TEXT("Catan LAN Lobby"), TEXT("LAN-лобби Catan")},
        {TEXT("Ready to host or join a game"), TEXT("Можно создавать игру или подключаться")},
        {TEXT("Waiting for the dedicated server..."), TEXT("Ожидание выделенного сервера...")},
        {TEXT("Enter server IP and optional port"), TEXT("Введите IP сервера и при необходимости порт")},
        {TEXT("Malformed create response"), TEXT("Некорректный ответ при создании игры")},
        {TEXT("Malformed join response"), TEXT("Некорректный ответ при подключении")},
        {TEXT("Saved dedicated server address is invalid"), TEXT("Сохранённый адрес сервера некорректен")},
        {TEXT("Saved dedicated credentials are incomplete"), TEXT("Сохранённые данные подключения неполны")},
        {TEXT("Malformed resume response"), TEXT("Некорректный ответ при восстановлении")},
        {TEXT("No saved dedicated session"), TEXT("Нет сохранённой серверной сессии")},
        {TEXT("Your public name is not part of this saved game"),
            TEXT("Вашего публичного имени нет в сохранённой игре")},
        {TEXT("LAN session service is unavailable"), TEXT("Сервис локальной сети недоступен")},
        {TEXT("Creating LAN lobby..."), TEXT("Создание LAN-лобби...")},
        {TEXT("Could not start LAN lobby"), TEXT("Не удалось создать LAN-лобби")},
        {TEXT("LAN search is already running..."), TEXT("Поиск в локальной сети уже выполняется...")},
        {TEXT("Could not start LAN discovery socket"), TEXT("Не удалось запустить поиск LAN-лобби")},
        {TEXT("Searching the local network..."), TEXT("Поиск в локальной сети...")},
        {TEXT("Select a discovered lobby first"), TEXT("Сначала выберите найденное лобби")},
        {TEXT("Joining lobby..."), TEXT("Подключение к лобби...")},
        {TEXT("Could not join the selected lobby"), TEXT("Не удалось войти в выбранное лобби")},
        {TEXT("Could not resolve lobby address"), TEXT("Не удалось определить адрес лобби")},
        {TEXT("Enter host IP address"), TEXT("Введите IP-адрес хоста")},
        {TEXT("Returned to main menu"), TEXT("Возврат в главное меню")},
        {TEXT("Connecting to dedicated server"), TEXT("Подключение к выделенному серверу")},
        {TEXT("Joining dedicated lobby on"), TEXT("Вход в серверное лобби на")},
        {TEXT("Reconnecting to dedicated server"), TEXT("Повторное подключение к серверу")},
        {TEXT("Connecting to"), TEXT("Подключение к")},
        {TEXT("Connection failed:"), TEXT("Ошибка подключения:")},
        {TEXT("Found"), TEXT("Найдено")},
        {TEXT("Dedicated lobby"), TEXT("Серверное лобби")},
        {TEXT("share token"), TEXT("токен для подключения")},
        {TEXT("WINS!"), TEXT("ПОБЕДИЛ!")}, {TEXT("FINAL SCORE"), TEXT("ИТОГОВЫЙ СЧЁТ")},
        {TEXT("VP"), TEXT("ПО")}, {TEXT("VP cards"), TEXT("карт ПО")},
        {TEXT("dev cards"), TEXT("карт развития")},
        {TEXT("final resource cards"), TEXT("карт ресурсов")},
        {TEXT("Remaining:"), TEXT("Осталось:")},
        {TEXT("offers to"), TEXT("предлагает игроку")},
        {TEXT("and requests:"), TEXT("и запрашивает:")},
        {TEXT("Saved LAN game restored"), TEXT("Сохранённая LAN-игра восстановлена")},
        {TEXT("Settlement built"), TEXT("Поселение построено")},
        {TEXT("Road built"), TEXT("Дорога построена")},
        {TEXT("City built"), TEXT("Замок построен")},
        {TEXT("Robber moved"), TEXT("Разбойник перемещён")},
        {TEXT("Robber moved and a resource was stolen"),
            TEXT("Разбойник перемещён, ресурс украден")},
        {TEXT("Resources discarded"), TEXT("Ресурсы сброшены")},
        {TEXT("Dice rolled"), TEXT("Кубики брошены")},
        {TEXT("Development card bought"), TEXT("Карта развития куплена")},
        {TEXT("Turn passed"), TEXT("Ход завершён")},
        {TEXT("Development card played"), TEXT("Карта развития сыграна")},
        {TEXT("Bank trade completed"), TEXT("Обмен с банком завершён")},
        {TEXT("Trade cancelled"), TEXT("Предложение обмена отменено")},
        {TEXT("Game started"), TEXT("Игра началась")},
        {TEXT("Single-player game started with"), TEXT("Игра с ботами запущена:")},
        {TEXT("event one bot"), TEXT("бот")}, {TEXT("event multiple bots"), TEXT("бота")},
        {TEXT("Trade offered to"), TEXT("Обмен предложен игроку")},
        {TEXT(" accepted the trade"), TEXT(" принял предложение обмена")},
        {TEXT(" declined the trade"), TEXT(" отклонил предложение обмена")},
        {TEXT("event resource cards"), TEXT("карт ресурсов")},
        {TEXT(" claimed Largest Army"), TEXT(" получил награду «Самая большая армия»")},
        {TEXT(" lost Largest Army"), TEXT(" потерял награду «Самая большая армия»")},
        {TEXT(" claimed Longest Road"), TEXT(" получил награду «Самая длинная дорога»")},
        {TEXT(" lost Longest Road"), TEXT(" потерял награду «Самая длинная дорога»")},
        {TEXT(" left the lobby"), TEXT(" покинул лобби")}
    };
};
}

const ICatanTextResources& FCatanTextResources::For(ECatanLanguage Language)
{
    static const FCatanEnglishTextResources English;
    static const FCatanRussianTextResources Russian;
    return Language == ECatanLanguage::Russian ? static_cast<const ICatanTextResources&>(Russian)
                                                : static_cast<const ICatanTextResources&>(English);
}

FString FCatanTextResources::Get(ECatanLanguage Language, const FString& Key)
{
    return For(Language).Get(Key);
}

bool FCatanTextResources::HasTranslation(ECatanLanguage Language, const FString& Key)
{
    return For(Language).Has(Key);
}

TArray<FString> FCatanTextResources::MissingTranslations(ECatanLanguage Language,
    const TArray<FString>& Keys)
{
    TArray<FString> Missing;
    for (const FString& Key : Keys)
        if (!HasTranslation(Language, Key)) Missing.Add(Key);
    return Missing;
}

FString FCatanTextResources::LanguageCode(ECatanLanguage Language)
{
    return Language == ECatanLanguage::Russian ? TEXT("ru") : TEXT("en");
}

ECatanLanguage FCatanTextResources::ParseLanguage(const FString& Code)
{
    return Code.Equals(TEXT("ru"), ESearchCase::IgnoreCase)
        || Code.Equals(TEXT("russian"), ESearchCase::IgnoreCase)
        ? ECatanLanguage::Russian : ECatanLanguage::English;
}

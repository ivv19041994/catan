#include "CatanTextResources.h"

namespace
{
class FCatanEnglishTextResources final : public ICatanTextResources
{
public:
    FString Get(const FString& Key) const override { return Key; }
};

class FCatanRussianTextResources final : public ICatanTextResources
{
public:
    FString Get(const FString& Key) const override
    {
        if (const FString* Value = Values.Find(Key)) return *Value;
        return Key;
    }

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
        {TEXT("Wood"), TEXT("Дерево")}, {TEXT("Clay"), TEXT("Глина")},
        {TEXT("Hay"), TEXT("Зерно")}, {TEXT("Sheep"), TEXT("Овцы")},
        {TEXT("Stone"), TEXT("Руда")},
        {TEXT("SHOW PLAYERS & COSTS"), TEXT("ИГРОКИ И ЦЕНЫ")},
        {TEXT("HIDE PLAYERS & COSTS"), TEXT("СКРЫТЬ ПОДРОБНОСТИ")},
        {TEXT("Development: 0"), TEXT("Карты развития: 0")},
        {TEXT("PLAYERS"), TEXT("ИГРОКИ")}, {TEXT("BUILD COSTS"), TEXT("СТОИМОСТЬ СТРОИТЕЛЬСТВА")},
        {TEXT("Costs appear for the current player"), TEXT("Здесь появится стоимость для текущего игрока")},
        {TEXT("ACTIONS"), TEXT("ДЕЙСТВИЯ")}, {TEXT("ROLL DICE"), TEXT("БРОСИТЬ КУБИКИ")},
        {TEXT("ROLL"), TEXT("БРОСОК")}, {TEXT("SETTLEMENT"), TEXT("ПОСЕЛЕНИЕ")},
        {TEXT("SETTLE"), TEXT("ПОСЕЛЕНИЕ")}, {TEXT("ROAD"), TEXT("ДОРОГА")},
        {TEXT("CITY"), TEXT("ЗАМОК")}, {TEXT("BUY DEV"), TEXT("КУПИТЬ КАРТУ")},
        {TEXT("USE DEV"), TEXT("ИГРАТЬ КАРТУ")}, {TEXT("VIEW DEV"), TEXT("КАРТЫ")},
        {TEXT("DEV"), TEXT("КАРТЫ")}, {TEXT("TRADE"), TEXT("ТОРГОВЛЯ")},
        {TEXT("END TURN"), TEXT("ЗАВЕРШИТЬ ХОД")}, {TEXT("END"), TEXT("ЗАВЕРШИТЬ")},
        {TEXT("DISCARD RESOURCES"), TEXT("СБРОС РЕСУРСОВ")},
        {TEXT("CONFIRM DISCARD"), TEXT("ПОДТВЕРДИТЬ СБРОС")},
        {TEXT("CHOOSE A PLAYER TO STEAL FROM"), TEXT("ВЫБЕРИТЕ ИГРОКА ДЛЯ КРАЖИ")},
        {TEXT("Player"), TEXT("Игрок")}, {TEXT("DEVELOPMENT CARDS"), TEXT("КАРТЫ РАЗВИТИЯ")},
        {TEXT("PLAY KNIGHT"), TEXT("СЫГРАТЬ РЫЦАРЯ")},
        {TEXT("PLAY ROAD BUILDING"), TEXT("СЫГРАТЬ СТРОИТЕЛЬСТВО ДОРОГ")},
        {TEXT("Resources for Year of Plenty / Monopoly"), TEXT("Ресурсы для Изобилия / Монополии")},
        {TEXT("PLAY YEAR OF PLENTY"), TEXT("СЫГРАТЬ ИЗОБИЛИЕ")},
        {TEXT("PLAY MONOPOLY (FIRST RESOURCE)"), TEXT("СЫГРАТЬ МОНОПОЛИЮ (ПЕРВЫЙ РЕСУРС)")},
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
        {TEXT("LOCAL NETWORK"), TEXT("ЛОКАЛЬНАЯ СЕТЬ")},
        {TEXT("DEDICATED SERVER"), TEXT("ВЫДЕЛЕННЫЙ СЕРВЕР")}, {TEXT("BACK"), TEXT("НАЗАД")},
        {TEXT("Host a LAN lobby, find one automatically, or enter its address."), TEXT("Создайте LAN-лобби, найдите его автоматически или введите адрес.")},
        {TEXT("Lobby name"), TEXT("Название лобби")}, {TEXT("HOST ONLINE (LAN)"), TEXT("СОЗДАТЬ LAN-ЛОББИ")},
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
        {TEXT("PLAYER NAME"), TEXT("ИМЯ")}, {TEXT("LANGUAGE"), TEXT("ЯЗЫК")},
        {TEXT("SAVE SETTINGS"), TEXT("СОХРАНИТЬ НАСТРОЙКИ")},
        {TEXT("Player name"), TEXT("Имя игрока")}, {TEXT("English"), TEXT("English")},
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
        {TEXT("Roll dice"), TEXT("Бросок кубиков")}, {TEXT("Build and trade"), TEXT("Строительство и торговля")},
        {TEXT("Discard resources"), TEXT("Сброс ресурсов")}, {TEXT("Move robber"), TEXT("Перемещение разбойника")},
        {TEXT("Road Building card"), TEXT("Карта строительства дорог")},
        {TEXT("Finished"), TEXT("Завершено")}, {TEXT("Unknown phase"), TEXT("Неизвестная фаза")},
        {TEXT("Current:"), TEXT("Сейчас ходит:")}, {TEXT("Dice:"), TEXT("Кубики:")},
        {TEXT("Waiting for"), TEXT("Ожидание игрока")},
        {TEXT("RESOURCE CARDS"), TEXT("КАРТ РЕСУРСОВ")},
        {TEXT("Knight"), TEXT("Рыцарь")}, {TEXT("Roads"), TEXT("Дороги")},
        {TEXT("Plenty"), TEXT("Изобилие")}, {TEXT("Monopoly"), TEXT("Монополия")},
        {TEXT("ready next turn"), TEXT("будут доступны в следующий ход")},
        {TEXT("YOUR RESOURCES:"), TEXT("ВАШИ РЕСУРСЫ:")},
        {TEXT("LONGEST ROAD"), TEXT("САМАЯ ДЛИННАЯ ДОРОГА")},
        {TEXT("LARGEST ARMY"), TEXT("САМАЯ БОЛЬШАЯ АРМИЯ")},
        {TEXT("YOU"), TEXT("ВЫ")}, {TEXT("BOT"), TEXT("БОТ")},
        {TEXT("Pieces:"), TEXT("Фигуры:")}, {TEXT("settlements"), TEXT("поселений")},
        {TEXT("cities"), TEXT("замков")}, {TEXT("roads"), TEXT("дорог")},
        {TEXT("DEV CARD"), TEXT("КАРТА РАЗВИТИЯ")},
        {TEXT("DISCARD"), TEXT("СБРОСИТЬ")}, {TEXT("RESOURCES"), TEXT("РЕСУРСОВ")},
        {TEXT("Ready to play:"), TEXT("Готово к использованию:")},
        {TEXT("Bought this turn:"), TEXT("Куплено в этот ход:")},
        {TEXT("available next turn."), TEXT("доступно в следующий ход.")},
        {TEXT("Victory point cards:"), TEXT("Карты победных очков:")},
        {TEXT("passive, they are never played."), TEXT("пассивные, их не нужно разыгрывать.")},
        {TEXT("Buy a random development card?"), TEXT("Купить случайную карту развития?")},
        {TEXT("This costs 1 hay, 1 sheep and 1 ore."), TEXT("Стоимость: 1 зерно, 1 овца и 1 руда.")},
        {TEXT("Roll the dice before building or trading."), TEXT("Перед строительством или торговлей бросьте кубики.")},
        {TEXT("All purchases are affordable. Choose an action."), TEXT("Доступны все покупки. Выберите действие.")},
        {TEXT("Need more resources for:"), TEXT("Нужно больше ресурсов для:")},
        {TEXT("road"), TEXT("дороги")}, {TEXT("settlement"), TEXT("поселения")},
        {TEXT("city"), TEXT("замка")}, {TEXT("development card"), TEXT("карты развития")},
        {TEXT("You have no development cards available to play."), TEXT("У вас нет карт развития, доступных для использования.")},
        {TEXT("Build a settlement: wood + clay + hay + sheep"), TEXT("Поселение: дерево + глина + зерно + овца")},
        {TEXT("Build a road: wood + clay"), TEXT("Дорога: дерево + глина")},
        {TEXT("Upgrade your settlement to a city: 2 hay + 3 ore"), TEXT("Улучшить поселение до замка: 2 зерна + 3 руды")},
        {TEXT("Buy a development card: hay + sheep + ore"), TEXT("Карта развития: зерно + овца + руда")},
        {TEXT("Move the robber and steal from an adjacent player"), TEXT("Переместить разбойника и украсть ресурс у соседнего игрока")},
        {TEXT("Place two roads for free"), TEXT("Бесплатно построить две дороги")},
        {TEXT("Take the two selected resources"), TEXT("Получить два выбранных ресурса")},
        {TEXT("Take the selected resource from every opponent"), TEXT("Забрать выбранный ресурс у всех соперников")}
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

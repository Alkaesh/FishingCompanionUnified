// ============================================================================
//  Statistics — модель данных приложения (отделена от GUI).
// ----------------------------------------------------------------------------
//  GUI только читает/пишет эти поля. Реальное наполнение (например, чтение
//  показателей из разрешённого источника) реализуется в Update() — заглушка.
// ============================================================================

#pragma once

namespace fc {

class Statistics
{
public:
    static Statistics& Get();

    // Обновление показателей. Вызывайте из RenderFrame() или фонового потока.
    void Update();

    // --- Показатели для дашборда ---
    int   fishCaught     = 0;       // поймано рыбы
    float currentWeight  = 0.0f;    // текущий вес, кг
    float bestCatch      = 0.0f;    // лучший улов, кг
    int   sessionMinutes = 0;       // длительность сессии, минут

    // --- Флаги отображения элементов HUD ---
    bool showRadar        = false;
    bool soundAlerts      = true;
    bool showCatchCounter = true;

    // --- Настройки таймеров ---
    int   sessionTimerMin   = 60;   // напоминание о длительности сессии
    int   notifyIntervalSec = 120;  // интервал уведомлений
    float notifyVolume      = 0.5f; // громкость уведомлений
    bool  breakReminder     = true; // напоминать о перерыве

private:
    Statistics() = default;
};

} // namespace fc

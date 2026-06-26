// ============================================================================
//  Statistics.cpp — заглушка слоя данных.
// ============================================================================

#include "Statistics.h"

namespace fc {

Statistics& Statistics::Get()
{
    static Statistics instance;
    return instance;
}

void Statistics::Update()
{
    // TODO: здесь заполняйте поля реальными значениями из вашего источника
    //       данных (разрешённый API игры, лог-файл сессии, сетевой канал и т.п.).
    //
    // Пример (демо-инкремент, чтобы убедиться, что UI «живой»):
    //   fishCaught++;
    //   currentWeight = 1.5f + (fishCaught % 5) * 0.3f;
    //   if (currentWeight > bestCatch) bestCatch = currentWeight;
}

} // namespace fc

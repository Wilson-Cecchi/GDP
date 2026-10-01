#include "game/Localization.hpp"

#include <array>

namespace gdp::game {
namespace {
struct Translation { std::string_view key; std::string_view portuguese; };
constexpr auto catalog = std::to_array<Translation>({
    {"INDUSTRIAL COMPLEX", "COMPLEXO INDUSTRIAL"},
    {"TEST MODE", "MODO DE TESTE"}, {"CAMPAIGN / STORY", "CAMPANHA / HISTÓRIA"},
    {"SETTINGS", "CONFIGURAÇÕES"}, {"QUIT GAME", "SAIR DO JOGO"},
    {"3 SAVES  -  10 FLOORS", "3 SAVES  -  10 ANDARES"},
    {"CAMPAIGN - SLOT", "CAMPANHA - SLOT"}, {"CAMPAIGN", "CAMPANHA"},
    {"UNAVAILABLE SAVE", "SAVE INDISPONÍVEL"}, {"EMPTY", "VAZIO"},
    {"CHECKPOINT: START OF EACH FLOOR", "SAVE NO INÍCIO DE CADA ANDAR"},
    {"CONTINUE", "CONTINUAR"}, {"NEW GAME", "NOVO JOGO"}, {"BACK", "VOLTAR"},
    {"BASE DIFFICULTY", "DIFICULDADE BASE"},
    {"THREAT INCREASES EACH FLOOR", "DIFICULDADE AUMENTA A CADA ANDAR"},
    {"EASY", "FÁCIL"}, {"NORMAL", "NORMAL"}, {"HARD", "DIFÍCIL"},
    {"REPLACE SLOT", "SUBSTITUIR SLOT"},
    {"THE PREVIOUS SAVE WILL BE REPLACED", "O SAVE ANTERIOR SERÁ SUBSTITUÍDO"},
    {"CANCEL", "CANCELAR"}, {"START NEW GAME", "INICIAR NOVO JOGO"},
    {"HARD ONLY - TIMER RUNS WHILE PAUSED", "DIFÍCIL - TEMPO CORRE DURANTE A PAUSA"},
    {"START RUN", "INICIAR PARTIDA"}, {"LEADERBOARD", "CLASSIFICAÇÃO"},
    {"LOCAL LEADERBOARD", "CLASSIFICAÇÃO LOCAL"}, {"BEST SEED", "MELHOR SEED"},
    {"TOP 10 - HARD - THIS COMPUTER", "TOP 10 - DIFÍCIL - NESTE COMPUTADOR"},
    {"NO FINISHED RUNS YET", "NENHUMA PARTIDA CONCLUÍDA"},
    {"CHANGES ARE SAVED AUTOMATICALLY", "ALTERAÇÕES SALVAS AUTOMATICAMENTE"},
    {"DISPLAY", "TELA"}, {"AUDIO", "ÁUDIO"}, {"CONTROLS", "CONTROLES"},
    {"LANGUAGE", "IDIOMA"}, {"ENGLISH", "ENGLISH"},
    {"FIELD OF VIEW", "CAMPO DE VISÃO"}, {"GRAPHICS", "GRÁFICOS"}, {"QUALITY", "QUALIDADE"},
    {"RESOLUTION", "RESOLUÇÃO"}, {"FULLSCREEN", "TELA CHEIA"},
    {"ON", "LIGADO"}, {"OFF", "DESLIGADO"},
    {"POTATO", "BATATA"}, {"LOW", "BAIXO"}, {"MEDIUM", "MÉDIO"}, {"HIGH", "ALTO"},
    {"MASTER", "GERAL"}, {"EFFECTS", "EFEITOS"}, {"AMBIENT", "AMBIENTE"},
    {"UI VOLUME", "VOLUME DA INTERFACE"}, {"UI", "INTERFACE"},
    {"SENSITIVITY", "SENSIBILIDADE"}, {"INVERT MOUSE Y", "INVERTER EIXO Y"},
    {"INVERT Y", "INVERTER Y"}, {"REMAP KEYS / MOUSE", "REMAPEAR TECLAS / MOUSE"},
    {"ESC / F3 RESERVED - MENU KEYS STAY FIXED", "ESC/F3 RESERVADOS - TECLAS FIXAS NOS MENUS"},
    {"KEY BINDINGS 1/2", "TECLAS 1/2"}, {"KEY BINDINGS 2/2", "TECLAS 2/2"},
    {"KEY / MOUSE / WHEEL - ESC CANCEL", "TECLA / MOUSE / SCROLL - ESC CANCELA"},
    {"SELECT TO REMAP - DUPLICATES SWAP", "SELECIONE - TECLA REPETIDA TROCA AÇÕES"},
    {"NEXT PAGE", "PRÓXIMA PÁGINA"}, {"PREVIOUS PAGE", "PÁGINA ANTERIOR"},
    {"RESET DEFAULTS", "RESTAURAR PADRÃO"},
    {"DEFAULT BINDINGS RESTORED", "CONTROLES PADRÃO RESTAURADOS"},
    {"MOVE", "MOVER"}, {"PERFORMANCE", "DESEMPENHO"},
    {"FORWARD", "AVANÇAR"}, {"BACKWARD", "RECUAR"},
    {"STRAFE LEFT", "MOVER À ESQUERDA"}, {"STRAFE RIGHT", "MOVER À DIREITA"},
    {"QUIET WALK", "ANDAR SILENCIOSO"}, {"CROUCH", "AGACHAR"}, {"JUMP", "PULAR"},
    {"FIRE", "ATIRAR"}, {"AIM", "MIRAR"}, {"RELOAD", "RECARREGAR"},
    {"INTERACT", "INTERAGIR"}, {"FLASHLIGHT", "LANTERNA"}, {"DROP WEAPON", "LARGAR ARMA"},
    {"PRIMARY WEAPON", "ARMA PRIMÁRIA"}, {"SECONDARY WEAPON", "ARMA SECUNDÁRIA"},
    {"MELEE", "CORPO A CORPO"}, {"EQUIPMENT (RESERVED)", "EQUIPAMENTO (RESERVADO)"},
    {"SPECIAL ITEM (RESERVED)", "ITEM ESPECIAL (RESERVADO)"},
    {"LAST WEAPON", "ÚLTIMA ARMA"}, {"INFORMATION", "INFORMAÇÕES"},
    {"NEXT WEAPON", "PRÓXIMA ARMA"}, {"PREVIOUS WEAPON", "ARMA ANTERIOR"},
    {"EXIT GAME?", "SAIR DO JOGO?"}, {"EXIT GAME", "SAIR DO JOGO"}, {"EXIT", "SAIR"},
    {"PAUSED", "PAUSADO"}, {"RESUME", "CONTINUAR"}, {"RESTART RUN", "REINICIAR PARTIDA"},
    {"MAIN MENU", "MENU PRINCIPAL"}, {"FLOOR CLEAR", "ANDAR CONCLUÍDO"},
    {"CAMPAIGN CLEAR", "CAMPANHA CONCLUÍDA"}, {"VICTORY", "VITÓRIA"}, {"DEFEAT", "DERROTA"},
    {"NEW RECORD", "NOVO RECORDE"}, {"NEXT FLOOR", "PRÓXIMO ANDAR"},
    {"NEW RUN", "NOVA PARTIDA"}, {"NEXT SEED", "PRÓXIMA SEED"},
    {"REPLAY FLOOR", "REPETIR ANDAR"}, {"REPLAY SEED", "REPETIR SEED"},
    {"HEALTH", "VIDA"}, {"ARMOR", "COLETE"}, {"KEYS", "CHAVES"},
    {"KNIFE", "FACA"}, {"CARBINE", "CARABINA"}, {"PISTOL", "PISTOLA"},
    {"READY", "PRONTA"}, {"LV", "NV"}, {"LIGHT ON", "LANTERNA LIGADA"},
    {"LOCKDOWN", "CONFINAMENTO"}, {"EVENT STANDBY", "EVENTO EM ESPERA"},
    {"EVENT ACTIVE", "EVENTO ATIVO"}, {"EVENT CLEAR", "EVENTO CONCLUÍDO"},
    {"OPEN DOOR", "ABRIR PORTA"}, {"DOOR LOCKED", "PORTA TRANCADA"},
    {"USE TERMINAL", "USAR TERMINAL"}, {"SUPPLIES ACQUIRED", "SUPRIMENTOS OBTIDOS"},
    {"EVENT START", "EVENTO INICIADO"}, {"NEW WAVE", "NOVA ONDA"},
    {"SECTOR CLEAR", "SETOR CONCLUÍDO"}, {"LEVEL UP", "NOVO NÍVEL"},
    {"WEAPON DROPPED", "ARMA LARGADA"}, {"KNIFE CANNOT DROP", "NÃO PODE LARGAR A FACA"},
    {"NEED SPACE / WAIT", "SEM ESPAÇO / AGUARDE"}, {"RUN STATUS", "DADOS DA PARTIDA"},
    {"SCORE", "PONTOS"}, {"KILLS", "ABATES"}, {"EVENTS", "EVENTOS"},
    {"LOOT", "COLETAS"}, {"TIME", "TEMPO"}, {"FLOOR", "ANDAR"},
    {"THREAT", "AMEAÇA"}, {"MAP", "MAPA"}, {"OBJECTIVE", "OBJETIVO"},
    {"REACH BOSS", "CHEGUE AO CHEFE"}, {"DEFEAT BOSS", "DERROTE O CHEFE"},
    {"TEST ARENA", "ÁREA DE TESTES"}, {"BOSS", "CHEFE"},
    {"PHASE 1/3 RANGE", "FASE 1/3 DISTÂNCIA"}, {"PHASE 2/3 PRESSURE", "FASE 2/3 PRESSÃO"},
    {"PHASE 3/3 PURSUIT", "FASE 3/3 PERSEGUIÇÃO"}, {"BEST", "MELHOR"},
    {"ARROWS", "SETAS"}, {"SELECT", "SELECIONE"}, {"ADJUST", "AJUSTE"},
    {"CONFIRM", "CONFIRME"}, {"MENUS", "MENUS"}, {"PAUSE / MENU", "PAUSA / MENU"},
    {"RUN INFO", "DADOS DA PARTIDA"}, {"DROP GUN", "LARGAR ARMA"},
    {"LAST / CYCLE WEAPON", "ÚLTIMA / TROCAR ARMA"},
    {"Run", "Partida"}, {"Ammo", "Munição"}, {"Keys", "Chaves"},
    {"Lv", "Nv"}, {"Score", "Pontos"}, {"Floor", "Andar"}, {"Time", "Tempo"},
    {"Best", "Melhor"}, {"Room", "Sala"}, {"Event", "Evento"},
    {"Lockdown", "Confinamento"}, {"SupplyDrop", "Suprimentos"},
    {"Ambush", "Emboscada"}, {"Recovery", "Recuperação"},
    {"Standby", "Espera"}, {"Active", "Ativo"}, {"Completed", "Concluído"},
});
constexpr bool word(const unsigned char c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
        || (c >= '0' && c <= '9') || c == '_' || c >= 128U;
}
} // namespace

std::string localize(const gameplay::Language language, const std::string_view source)
{
    if (language != gameplay::Language::PortugueseBrazil) return std::string{source};
    for (const auto& item : catalog) if (source == item.key) return std::string{item.portuguese};
    std::string result;
    result.reserve(source.size());
    // Longest catalog phrase at whole-word boundaries covers numeric HUD/saves labels.
    // Bracketed bindings stay literal: [SPACE], [WHEEL UP], custom keys, etc.
    bool binding = false;
    for (std::size_t i = 0U; i < source.size();) {
        if (source[i] == '[') binding = true;
        if (source[i] == ']') binding = false;
        const Translation* match = nullptr;
        if (!binding && (i == 0U || !word(static_cast<unsigned char>(source[i - 1U])))) {
            for (const auto& item : catalog) {
                const auto end = i + item.key.size();
                if (source.substr(i).starts_with(item.key)
                    && (end == source.size() || !word(static_cast<unsigned char>(source[end])))
                    && (!match || item.key.size() > match->key.size())) match = &item;
            }
        }
        if (match) { result += match->portuguese; i += match->key.size(); }
        else result += source[i++];
    }
    return result;
}

std::uint32_t nextUiCodepoint(const std::string_view text, std::size_t& offset) noexcept
{
    if (offset >= text.size()) return 0U;
    const auto first = static_cast<unsigned char>(text[offset++]);
    if (first < 128U) return first;
    const unsigned count = first >= 0xC2U && first <= 0xDFU ? 1U
        : first >= 0xE0U && first <= 0xEFU ? 2U : first >= 0xF0U && first <= 0xF4U ? 3U : 0U;
    if (count == 0U || text.size() - offset < count) return '?';
    std::uint32_t codepoint = first & ((1U << (6U - count)) - 1U);
    for (unsigned i = 0U; i < count; ++i) {
        const auto byte = static_cast<unsigned char>(text[offset + i]);
        if ((byte & 0xC0U) != 0x80U) return '?';
        codepoint = (codepoint << 6U) | (byte & 0x3FU);
    }
    if ((count == 1U && codepoint < 0x80U) || (count == 2U && codepoint < 0x800U)
        || (count == 3U && codepoint < 0x10000U) || codepoint > 0x10FFFFU
        || (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) return '?';
    offset += count;
    return codepoint;
}

std::size_t uiTextLength(const std::string_view text) noexcept
{
    std::size_t count = 0U, offset = 0U;
    while (offset < text.size()) { (void)nextUiCodepoint(text, offset); ++count; }
    return count;
}

void popUiCodepoint(std::string& text) noexcept
{
    if (text.empty()) return;
    auto start = text.size() - 1U;
    while (start > 0U && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U) --start;
    text.resize(start);
}
} // namespace gdp::game

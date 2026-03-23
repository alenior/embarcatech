const {onSchedule} = require("firebase-functions/v2/scheduler");
const admin = require("firebase-admin");
const axios = require("axios");

admin.initializeApp();

const API_KEY = process.env.APISPORTS_KEY;

console.log("API KEY carregada:", !!API_KEY);

// =========================
// INTERVALOS (ms)
// =========================
const INTERVALS = {
  idle: 60 * 60 * 1000, // 1h
  pre: 15 * 60 * 1000, // 15 min
  live: 75 * 1000, // 1 min 15s
};

// =========================
// FUNÇÃO AUXILIAR
// =========================
/**
 * Retorna a data atual no formato YYYY-MM-DD,
 * utilizado pela API-Football para consultas por dia.
 *
 * @return {string} Data formatada (ex: 2026-03-22)
 */
function getTodayDate() {
  const d = new Date();
  return d.toISOString().split("T")[0];
}

/**
 * Handler principal da função agendada.
 *
 * Responsável por:
 * - Consultar jogos do dia do time configurado
 * - Detectar estado do jogo (idle, pre, live)
 * - Controlar consumo da API
 * - Armazenar dados no Firebase RTDB
 * - Buscar eventos apenas durante jogos ao vivo
 */
async function fetchLiveMatchesHandler() {
  const now = Date.now();
  const db = admin.database();

  console.log("==== EXECUÇÃO INICIADA ====");

  // =========================
  // 1. CONFIGURAÇÃO
  // =========================
  const configSnap = await db.ref("config").once("value");
  const config = configSnap.val();

  if (!config || !config.team_id) {
    console.error("Config inválida");
    return null;
  }

  const teamId = config.team_id;
  const lastCall = config.last_call || 0;

  // =========================
  // 2. CONTROLE DE INTERVALO
  // =========================
  let interval = INTERVALS.idle;

  if (config.current_mode === "live") interval = INTERVALS.live;
  else if (config.current_mode === "pre") interval = INTERVALS.pre;

  if (now - lastCall < interval) {
    console.log("⏱️ Aguardando intervalo...");
    return null;
  }

  console.log("Chamando API...");

  try {
    // =========================
    // 3. BUSCA JOGOS DO DIA
    // =========================
    const response = await axios.get(
        "https://v3.football.api-sports.io/fixtures",
        {
          params: {
            team: teamId,
            date: getTodayDate(),
          },
          headers: {
            "x-apisports-key": API_KEY,
          },
        },
    );

    // =========================
    // 4. TRATAMENTO DE ERRO API
    // =========================
    if (response.data.errors && Object.keys(response.data.errors).length > 0) {
      console.error("Erro da API:", response.data.errors);
      return null;
    }

    const fixtures = response.data.response;

    console.log("Fixtures encontrados:", fixtures.length);

    // =========================
    // 5. SEM JOGO HOJE
    // =========================
    if (fixtures.length === 0) {
      console.log("Nenhum jogo hoje");

      await db.ref("config").update({
        current_mode: "idle",
        last_call: now,
      });

      return null;
    }

    // =========================
    // 6. SELECIONA JOGO
    // =========================
    const match = fixtures[0];

    const matchId = match.fixture.id;
    const status = match.fixture.status.short;
    const startTime = new Date(match.fixture.date).getTime();

    console.log("Status:", status);

    // =========================
    // 7. DETECTA ESTADO
    // =========================
    let currentMode = "idle";

    if (status === "NS") {
      const diff = startTime - now;

      if (diff <= 5 * 60 * 60 * 1000) {
        currentMode = "pre";
      }
    } else if (
      status === "1H" ||
      status === "2H" ||
      status === "HT" ||
      status === "ET" ||
      status === "P"
    ) {
      currentMode = "live";
    }

    console.log("Modo atual:", currentMode);

    // =========================
    // 8. SALVA CONTROLE
    // =========================
    await db.ref("config").update({
      current_mode: currentMode,
      last_call: now,
    });

    const matchRef = db.ref(`matches/${matchId}`);

    // =========================
    // 9. SALVA DADOS DO JOGO
    // =========================
    await matchRef.update({
      home: match.teams.home.name,
      away: match.teams.away.name,
      score: `${match.goals.home}x${match.goals.away}`,
      status: status,
      league: match.league.name,
      country: match.league.country,
      start_time: match.fixture.date,
      elapsed: match.fixture.status.elapsed,
      last_update: now,
    });

    // =========================
    // 10. EVENTOS (AO VIVO)
    // =========================
    if (currentMode === "live") {
      console.log("Buscando eventos...");

      const eventsResponse = await axios.get(
          "https://v3.football.api-sports.io/fixtures/events",
          {
            params: {fixture: matchId},
            headers: {
              "x-apisports-key": API_KEY,
            },
          },
      );

      const events = eventsResponse.data.response;

      console.log("Eventos encontrados:", events.length);

      for (const ev of events) {
        const playerName =
          ev.player && ev.player.name ? ev.player.name : "unknown";

        const eventId = `${ev.time.elapsed}_${playerName}_${ev.type}`;

        const eventRef = matchRef.child(`events/${eventId}`);
        const snapshot = await eventRef.once("value");

        if (!snapshot.exists()) {
          await eventRef.set({
            type: ev.type,
            detail: ev.detail,
            team: ev.team.name,
            player: playerName,
            minute: ev.time.elapsed,
          });
        }
      }
    }
  } catch (error) {
    console.error("Erro geral:", error.message);
  }

  return null;
}

// =========================
// EXPORTAÇÃO (AGENDAMENTO)
// =========================
exports.fetchLiveMatches = onSchedule(
    "every 1 minutes",
    fetchLiveMatchesHandler,
);

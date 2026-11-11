using System;

namespace Orchard
{
    public static partial class World
    {
        static string J(params string[] parts) => string.Join(((char)31).ToString(), parts);
        static double[] Cat(double[] a, double[] b) { var r = new double[a.Length + (b?.Length ?? 0)]; a.CopyTo(r, 0); b?.CopyTo(r, a.Length); return r; }
        static Vec3 V3(double[] v) => v.Length >= 3 ? new Vec3(v[0], v[1], v[2]) : Vec3.Zero;
        static Vec3[] L3(double[] v) { var o = new Vec3[v.Length / 3]; for (int i = 0; i < o.Length; ++i) o[i] = new Vec3(v[i * 3], v[i * 3 + 1], v[i * 3 + 2]); return o; }

        public static void RandomSeed(double seed) { Native.Game(200, 0, 0, "", new double[] {seed}); }
        public static double RandomGaussian(double mean = 0.0, double deviation = 1.0) => Native.Game(201, 0, 0, "", new double[] {mean, deviation});
        public static bool RandomChance(double probability = 0.5) => Native.Game(202, 0, 0, "", new double[] {probability}) != 0.0;
        public static int RandomWeighted(params double[] weights) => (int)Native.Game(203, 0, 0, "", Cat(new double[] {}, weights));
        public static double Noise(double x, double y = 0.0, double z = 0.0, double seed = 0.0) => Native.Game(204, 0, 0, "", new double[] {x, y, z, seed});
        public static double FractalNoise(double x, double y = 0.0, double z = 0.0, double octaves = 4.0, double seed = 0.0) => Native.Game(205, 0, 0, "", new double[] {x, y, z, octaves, seed});
        public static Vec3 RandomInSphere(double radius = 1.0) => V3(Native.GameValues(206, 0, 0, "", new double[] {radius}));
        public static Vec3 RandomOnSphere(double radius = 1.0) => V3(Native.GameValues(207, 0, 0, "", new double[] {radius}));
        public static Vec3 RandomInCircle(double radius = 1.0) => V3(Native.GameValues(208, 0, 0, "", new double[] {radius}));
        public static Vec3 RandomInCone(double axis_x, double axis_y, double axis_z, double degrees = 30.0) => V3(Native.GameValues(209, 0, 0, "", new double[] {axis_x, axis_y, axis_z, degrees}));
        public static string RandomText(double length = 8.0, string alphabet = "abcdefghijklmnopqrstuvwxyz") => Native.GameText(210, 0, 0, J(alphabet), new double[] {length});
        public static void BagAdd(string bag, string item, double copies = 1.0) { Native.Game(211, 0, 0, J(bag, item), new double[] {copies}); }
        public static string BagNext(string bag) => Native.GameText(212, 0, 0, J(bag), new double[] {});
        public static string BagPeek(string bag) => Native.GameText(213, 0, 0, J(bag), new double[] {});
        public static int BagRemaining(string bag) => (int)Native.Game(214, 0, 0, J(bag), new double[] {});
        public static int BagSize(string bag) => (int)Native.Game(215, 0, 0, J(bag), new double[] {});
        public static void BagReset(string bag) { Native.Game(216, 0, 0, J(bag), new double[] {}); }
        public static void BagClear(string bag) { Native.Game(217, 0, 0, J(bag), new double[] {}); }
        public static bool BagRemove(string bag, string item) => Native.Game(218, 0, 0, J(bag, item), new double[] {}) != 0.0;
        public static void BagAvoidRepeats(string bag, bool on = true) { Native.Game(219, 0, 0, J(bag), new double[] {on ? 1.0 : 0.0}); }
        public static int PlaySound(string clip, double volume = 1.0, double pitch = 1.0) => (int)Native.Game(220, 0, 0, J(clip), new double[] {volume, pitch});
        public static int PlaySoundAt(string clip, double x, double y, double z, double volume = 1.0, double pitch = 1.0, double range = 25.0) => (int)Native.Game(221, 0, 0, J(clip), new double[] {x, y, z, volume, pitch, range});
        public static int PlaySoundOn(Entity entity, string clip, double volume = 1.0, double pitch = 1.0, double range = 25.0) => (int)Native.Game(222, entity?.Id ?? 0, 0, J(clip), new double[] {volume, pitch, range});
        public static int LoopSound(string clip, double volume = 1.0) => (int)Native.Game(223, 0, 0, J(clip), new double[] {volume});
        public static int LoopSoundOn(Entity entity, string clip, double volume = 1.0, double range = 25.0) => (int)Native.Game(224, entity?.Id ?? 0, 0, J(clip), new double[] {volume, range});
        public static void StopSound(double handle) { Native.Game(225, 0, 0, "", new double[] {handle}); }
        public static void StopAllSounds() { Native.Game(226, 0, 0, "", new double[] {}); }
        public static void SetSoundVolume(double handle, double volume) { Native.Game(227, 0, 0, "", new double[] {handle, volume}); }
        public static void SetSoundPitch(double handle, double pitch) { Native.Game(228, 0, 0, "", new double[] {handle, pitch}); }
        public static void PauseSound(double handle, bool paused = true) { Native.Game(229, 0, 0, "", new double[] {handle, paused ? 1.0 : 0.0}); }
        public static bool SoundPlaying(double handle) => Native.Game(230, 0, 0, "", new double[] {handle}) != 0.0;
        public static int PlaySoundSource(Entity entity) => (int)Native.Game(231, entity?.Id ?? 0, 0, "", new double[] {});
        public static void StopSoundSource(Entity entity) { Native.Game(232, entity?.Id ?? 0, 0, "", new double[] {}); }
        public static bool AiAdd(Entity entity, string brain = "guard") => Native.Game(233, entity?.Id ?? 0, 0, J(brain), new double[] {}) != 0.0;
        public static bool AiSetBrain(Entity entity, string brain) => Native.Game(234, entity?.Id ?? 0, 0, J(brain), new double[] {}) != 0.0;
        public static bool AiSetBehavior(Entity entity, string behavior) => Native.Game(235, entity?.Id ?? 0, 0, J(behavior), new double[] {}) != 0.0;
        public static bool AiDefineBehavior(string name, string behavior) => Native.Game(236, 0, 0, J(name, behavior), new double[] {}) != 0.0;
        public static string AiBrainBehavior(string brain) => Native.GameText(237, 0, 0, J(brain), new double[] {});
        public static string AiBehavior(Entity entity) => Native.GameText(238, entity?.Id ?? 0, 0, "", new double[] {});
        public static bool AiEnable(Entity entity, bool on = true) => Native.Game(239, entity?.Id ?? 0, 0, "", new double[] {on ? 1.0 : 0.0}) != 0.0;
        public static bool AiIsAgent(Entity entity) => Native.Game(240, entity?.Id ?? 0, 0, "", new double[] {}) != 0.0;
        public static bool AiSetTeam(Entity entity, string team) => Native.Game(241, entity?.Id ?? 0, 0, J(team), new double[] {}) != 0.0;
        public static string AiTeam(Entity entity) => Native.GameText(242, entity?.Id ?? 0, 0, "", new double[] {});
        public static void AiSetRelation(string team_a, string team_b, double relation) { Native.Game(243, 0, 0, J(team_a, team_b), new double[] {relation}); }
        public static int AiRelation(string team_a, string team_b) => (int)Native.Game(244, 0, 0, J(team_a, team_b), new double[] {});
        public static bool AiHostile(Entity entity, Entity other) => Native.Game(245, entity?.Id ?? 0, other?.Id ?? 0, "", new double[] {}) != 0.0;
        public static bool AiMoveTo(Entity entity, double x, double y, double z, bool run = false) => Native.Game(246, entity?.Id ?? 0, 0, "", new double[] {x, y, z, run ? 1.0 : 0.0}) != 0.0;
        public static void AiStop(Entity entity) { Native.Game(247, entity?.Id ?? 0, 0, "", new double[] {}); }
        public static bool AiArrived(Entity entity) => Native.Game(248, entity?.Id ?? 0, 0, "", new double[] {}) != 0.0;
        public static bool AiIsMoving(Entity entity) => Native.Game(249, entity?.Id ?? 0, 0, "", new double[] {}) != 0.0;
        public static bool AiSetTarget(Entity entity, Entity target) => Native.Game(250, entity?.Id ?? 0, target?.Id ?? 0, "", new double[] {}) != 0.0;
        public static Entity AiTarget(Entity entity) => new Entity(Native.GameEntity(251, entity?.Id ?? 0, 0, "", new double[] {}));
        public static bool AiCanSee(Entity entity, Entity other) => Native.Game(252, entity?.Id ?? 0, other?.Id ?? 0, "", new double[] {}) != 0.0;
        public static double AiAwareness(Entity entity, Entity other) => Native.Game(253, entity?.Id ?? 0, other?.Id ?? 0, "", new double[] {});
        public static double AiSuspicion(Entity entity) => Native.Game(254, entity?.Id ?? 0, 0, "", new double[] {});
        public static int AiAlertLevel(Entity entity) => (int)Native.Game(255, entity?.Id ?? 0, 0, "", new double[] {});
        public static string AiState(Entity entity) => Native.GameText(256, entity?.Id ?? 0, 0, "", new double[] {});
        public static string AiAction(Entity entity) => Native.GameText(257, entity?.Id ?? 0, 0, "", new double[] {});
        public static string AiEvent(Entity entity) => Native.GameText(258, entity?.Id ?? 0, 0, "", new double[] {});
        public static string AiPendingAction(Entity entity) => Native.GameText(259, entity?.Id ?? 0, 0, "", new double[] {});
        public static string AiPendingArgument(Entity entity, double index = 0.0) => Native.GameText(260, entity?.Id ?? 0, 0, "", new double[] {index});
        public static bool AiFinishAction(Entity entity, bool success = true) => Native.Game(261, entity?.Id ?? 0, 0, "", new double[] {success ? 1.0 : 0.0}) != 0.0;
        public static void AiSetVisibility(Entity entity, double factor) { Native.Game(262, entity?.Id ?? 0, 0, "", new double[] {factor}); }
        public static void AiMakeNoise(double x, double y, double z, double loudness = 15.0, string kind = "noise") { Native.Game(263, 0, 0, J(kind), new double[] {x, y, z, loudness}); }
        public static string AiTeamAlert(string team) => Native.GameText(264, 0, 0, J(team), new double[] {});
        public static void AiSetTeamAlert(string team, string phase, double x = 0.0, double y = 0.0, double z = 0.0) { Native.Game(265, 0, 0, J(team, phase), new double[] {x, y, z}); }
        public static Vec3[] AiFindPath(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) => L3(Native.GameValues(266, 0, 0, "", new double[] {from_x, from_y, from_z, to_x, to_y, to_z}));
        public static double AiPathLength(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) => Native.Game(267, 0, 0, "", new double[] {from_x, from_y, from_z, to_x, to_y, to_z});
        public static Vec3[] AiPath(Entity entity) => L3(Native.GameValues(268, entity?.Id ?? 0, 0, "", new double[] {}));
        public static Vec3 AiRandomPoint(double x, double y, double z, double radius = 10.0) => V3(Native.GameValues(269, 0, 0, "", new double[] {x, y, z, radius}));
        public static Vec3 AiSnapToGround(double x, double y, double z) => V3(Native.GameValues(270, 0, 0, "", new double[] {x, y, z}));
        public static int AiAddDanger(double x, double y, double z, double radius = 3.0, double amount = 5.0, double seconds = 0.0) => (int)Native.Game(271, 0, 0, "", new double[] {x, y, z, radius, amount, seconds});
        public static void AiClearDanger() { Native.Game(272, 0, 0, "", new double[] {}); }
        public static int AiBake(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) => (int)Native.Game(273, 0, 0, "", new double[] {from_x, from_y, from_z, to_x, to_y, to_z});
        public static double[] AiFindCover(Entity entity, double threat_x, double threat_y, double threat_z, double radius = 12.0) => Native.GameValues(274, entity?.Id ?? 0, 0, "", new double[] {threat_x, threat_y, threat_z, radius});
        public static bool AiLineOfSight(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) => Native.Game(275, 0, 0, "", new double[] {from_x, from_y, from_z, to_x, to_y, to_z}) != 0.0;
        public static Vec3 AiLastSeen(Entity entity) => V3(Native.GameValues(276, entity?.Id ?? 0, 0, "", new double[] {}));
        public static void AiSetNumber(Entity entity, string key, double value) { Native.Game(277, entity?.Id ?? 0, 0, J(key), new double[] {value}); }
        public static double AiGetNumber(Entity entity, string key) => Native.Game(278, entity?.Id ?? 0, 0, J(key), new double[] {});
        public static void AiSetText(Entity entity, string key, string value) { Native.Game(279, entity?.Id ?? 0, 0, J(key, value), new double[] {}); }
        public static string AiGetText(Entity entity, string key) => Native.GameText(280, entity?.Id ?? 0, 0, J(key), new double[] {});
        public static double AiDirectorIntensity() => Native.Game(281, 0, 0, "", new double[] {});
        public static double AiDirectorMenace() => Native.Game(282, 0, 0, "", new double[] {});
        public static string AiDirectorPhase() => Native.GameText(283, 0, 0, "", new double[] {});
        public static void AiDirectorNote(string habit, double amount = 1.0) { Native.Game(284, 0, 0, J(habit), new double[] {amount}); }
        public static double AiDirectorHabit(string habit) => Native.Game(285, 0, 0, J(habit), new double[] {});
        public static void AiSetSpawns(string entity_template, double max_alive = 10.0, double min_distance = 15.0, double max_distance = 40.0, double interval = 3.0) { Native.Game(286, 0, 0, J(entity_template), new double[] {max_alive, min_distance, max_distance, interval}); }
        public static int AiAgentCount() => (int)Native.Game(287, 0, 0, "", new double[] {});
        public static Entity AiMainPlayer() => new Entity(Native.GameEntity(288, 0, 0, "", new double[] {}));
        public static Vec3 AiLobVelocity(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double apex = 2.0) => V3(Native.GameValues(289, 0, 0, "", new double[] {from_x, from_y, from_z, to_x, to_y, to_z, apex}));
        public static Vec3 AiIntercept(double shooter_x, double shooter_y, double shooter_z, double target_x, double target_y, double target_z, double target_vx, double target_vy, double target_vz, double speed) => V3(Native.GameValues(290, 0, 0, "", new double[] {shooter_x, shooter_y, shooter_z, target_x, target_y, target_z, target_vx, target_vy, target_vz, speed}));
        public static int VoxelDefine(string name, bool solid = true, bool climbable = false, bool liquid = false, double cost = 1.0, double danger = 0.0, bool door = false) => (int)Native.Game(291, 0, 0, J(name), new double[] {solid ? 1.0 : 0.0, climbable ? 1.0 : 0.0, liquid ? 1.0 : 0.0, cost, danger, door ? 1.0 : 0.0});
        public static void VoxelSet(double x, double y, double z, string type) { Native.Game(292, 0, 0, J(type), new double[] {x, y, z}); }
        public static void VoxelFill(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, string type) { Native.Game(293, 0, 0, J(type), new double[] {from_x, from_y, from_z, to_x, to_y, to_z}); }
        public static string VoxelGet(double x, double y, double z) => Native.GameText(294, 0, 0, "", new double[] {x, y, z});
        public static void VoxelClear() { Native.Game(295, 0, 0, "", new double[] {}); }
        public static void VoxelSolidBelow(double y) { Native.Game(296, 0, 0, "", new double[] {y}); }
        public static Vec3[] VoxelPath(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double height = 2.0, double step = 1.0, double drop = 3.0, double jump = 0.0, double danger_weight = 10.0) => L3(Native.GameValues(297, 0, 0, "", new double[] {from_x, from_y, from_z, to_x, to_y, to_z, height, step, drop, jump, danger_weight}));
        public static Vec3[] VoxelFlee(double from_x, double from_y, double from_z, double threat_x, double threat_y, double threat_z, double distance = 10.0, double height = 2.0) => L3(Native.GameValues(298, 0, 0, "", new double[] {from_x, from_y, from_z, threat_x, threat_y, threat_z, distance, height}));
        public static void GoapAction(string planner, string name, string needs, string gives, double cost = 1.0) { Native.Game(299, 0, 0, J(planner, name, needs, gives), new double[] {cost}); }
        public static void GoapRemove(string planner, string name) { Native.Game(300, 0, 0, J(planner, name), new double[] {}); }
        public static void GoapClear(string planner) { Native.Game(301, 0, 0, J(planner), new double[] {}); }
        public static string GoapPlan(string planner, string start, string goal) => Native.GameText(302, 0, 0, J(planner, start, goal), new double[] {});
        public static int ChessNew(string fen = "") => (int)Native.Game(303, 0, 0, J(fen), new double[] {});
        public static void ChessFree(double game) { Native.Game(304, 0, 0, "", new double[] {game}); }
        public static bool ChessLoad(double game, string fen) => Native.Game(305, 0, 0, J(fen), new double[] {game}) != 0.0;
        public static string ChessFen(double game) => Native.GameText(306, 0, 0, "", new double[] {game});
        public static bool ChessMove(double game, string move) => Native.Game(307, 0, 0, J(move), new double[] {game}) != 0.0;
        public static bool ChessIsLegal(double game, string move) => Native.Game(308, 0, 0, J(move), new double[] {game}) != 0.0;
        public static bool ChessUndo(double game) => Native.Game(309, 0, 0, "", new double[] {game}) != 0.0;
        public static string ChessBestMove(double game, double level = 5.0) => Native.GameText(310, 0, 0, "", new double[] {game, level});
        public static string ChessStatus(double game) => Native.GameText(311, 0, 0, "", new double[] {game});
        public static int ChessTurn(double game) => (int)Native.Game(312, 0, 0, "", new double[] {game});
        public static string ChessMoves(double game) => Native.GameText(313, 0, 0, "", new double[] {game});
        public static int ChessPiece(double game, string square) => (int)Native.Game(314, 0, 0, J(square), new double[] {game});
        public static string ChessBoard(double game) => Native.GameText(315, 0, 0, "", new double[] {game});
        public static string ChessHistory(double game) => Native.GameText(316, 0, 0, "", new double[] {game});
        public static bool NnCreate(string name, string layers, string hidden = "tanh", string output = "sigmoid") => Native.Game(317, 0, 0, J(name, layers, hidden, output), new double[] {}) != 0.0;
        public static double[] NnRun(string name, params double[] inputs) => Native.GameValues(318, 0, 0, J(name), Cat(new double[] {}, inputs));
        public static double NnTrain(string name, double rate, params double[] inputs_then_targets) => Native.Game(319, 0, 0, J(name), Cat(new double[] {rate}, inputs_then_targets));
        public static string NnSave(string name) => Native.GameText(320, 0, 0, J(name), new double[] {});
        public static bool NnLoad(string name, string data) => Native.Game(321, 0, 0, J(name, data), new double[] {}) != 0.0;
        public static int QChoose(string name, string state, double actions = 4.0) => (int)Native.Game(322, 0, 0, J(name, state), new double[] {actions});
        public static int QBest(string name, string state) => (int)Native.Game(323, 0, 0, J(name, state), new double[] {});
        public static void QLearn(string name, string state, double action, double reward, string next, bool terminal = false) { Native.Game(324, 0, 0, J(name, state, next), new double[] {action, reward, terminal ? 1.0 : 0.0}); }
        public static void QSettings(string name, double epsilon = 0.1, double alpha = 0.2, double gamma = 0.9) { Native.Game(325, 0, 0, J(name), new double[] {epsilon, alpha, gamma}); }
        public static string QSave(string name) => Native.GameText(326, 0, 0, J(name), new double[] {});
        public static bool QLoad(string name, string data) => Native.Game(327, 0, 0, J(name, data), new double[] {}) != 0.0;
        public static void MarkovTrain(string name, string text, double order = 1.0, bool letters = false) { Native.Game(328, 0, 0, J(name, text), new double[] {order, letters ? 1.0 : 0.0}); }
        public static string MarkovGenerate(string name, double max_length = 20.0, bool letters = false) => Native.GameText(329, 0, 0, J(name), new double[] {max_length, letters ? 1.0 : 0.0});
        public static double RandomRange(double low = 0.0, double high = 1.0) => Native.Game(330, 0, 0, "", new double[] {low, high});
        public static int RandomInteger(double low = 0.0, double high = 10.0) => (int)Native.Game(331, 0, 0, "", new double[] {low, high});
        public static string RandomPick(string choices) => Native.GameText(332, 0, 0, J(choices), new double[] {});
        public static double RandomWhole(double low = 1.0, double high = 6.0) => Native.Game(333, 0, 0, "", new double[] {low, high});
        public static double BagCount(string bag) => Native.Game(334, 0, 0, J(bag), new double[] {});
        public static void AiMakeNoiseFrom(Entity entity, double loudness = 15.0, string kind = "noise") { Native.Game(335, entity?.Id ?? 0, 0, J(kind), new double[] {loudness}); }
        public static double SecureRandom(double low = 0.0, double high = 1.0) => Native.Game(336, 0, 0, "", new double[] {low, high});
        public static double SecureRandomInteger(double low = 0.0, double high = 100.0) => Native.Game(337, 0, 0, "", new double[] {low, high});
        public static bool SecureChance(double probability = 0.5) => Native.Game(338, 0, 0, "", new double[] {probability}) != 0.0;
        public static string SecurePick(string choices) => Native.GameText(339, 0, 0, J(choices), new double[] {});
        public static string SecureRandomText(double length = 16.0, string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") => Native.GameText(340, 0, 0, J(alphabet), new double[] {length});
        public static string SecureToken(double byte_count = 32.0) => Native.GameText(341, 0, 0, "", new double[] {byte_count});
        public static string SecureHex(double byte_count = 16.0) => Native.GameText(342, 0, 0, "", new double[] {byte_count});
        public static string SecureUuid() => Native.GameText(343, 0, 0, "", new double[] {});
    }
}

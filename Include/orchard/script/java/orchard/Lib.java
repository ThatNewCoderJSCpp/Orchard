package orchard;

public class Lib {
    protected Lib() {
    }

    static String libJoin(String... parts) {
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < parts.length; ++i) {
            if (i > 0) b.append((char) 31);
            b.append(parts[i] == null ? "" : parts[i]);
        }
        return b.toString();
    }

    static double[] libCat(double[] a, double[] b) {
        double[] r = new double[a.length + b.length];
        System.arraycopy(a, 0, r, 0, a.length);
        System.arraycopy(b, 0, r, a.length, b.length);
        return r;
    }

    static Vec3 libVec(double[] v) { return v.length >= 3 ? new Vec3(v[0], v[1], v[2]) : new Vec3(); }

    static Vec3[] libVecs(double[] v) {
        Vec3[] out = new Vec3[v.length / 3];
        for (int i = 0; i < out.length; ++i) out[i] = new Vec3(v[i * 3], v[i * 3 + 1], v[i * 3 + 2]);
        return out;
    }

    static long libId(Entity e) { return e == null ? 0 : e.id(); }

    public static void randomSeed(double seed) { Native.game(200, 0, 0, new double[] {seed}, ""); }
    public static double randomGaussian(double mean, double deviation) { return Native.game(201, 0, 0, new double[] {mean, deviation}, ""); }
    public static double randomGaussian(double mean) { return randomGaussian(mean, 1.0); }
    public static double randomGaussian() { return randomGaussian(0.0, 1.0); }
    public static boolean randomChance(double probability) { return Native.game(202, 0, 0, new double[] {probability}, "") != 0.0; }
    public static boolean randomChance() { return randomChance(0.5); }
    public static int randomWeighted(double... weights) { return (int) Native.game(203, 0, 0, libCat(new double[] {}, weights), ""); }
    public static double noise(double x, double y, double z, double seed) { return Native.game(204, 0, 0, new double[] {x, y, z, seed}, ""); }
    public static double noise(double x, double y, double z) { return noise(x, y, z, 0.0); }
    public static double noise(double x, double y) { return noise(x, y, 0.0, 0.0); }
    public static double noise(double x) { return noise(x, 0.0, 0.0, 0.0); }
    public static double fractalNoise(double x, double y, double z, double octaves, double seed) { return Native.game(205, 0, 0, new double[] {x, y, z, octaves, seed}, ""); }
    public static double fractalNoise(double x, double y, double z, double octaves) { return fractalNoise(x, y, z, octaves, 0.0); }
    public static double fractalNoise(double x, double y, double z) { return fractalNoise(x, y, z, 4.0, 0.0); }
    public static double fractalNoise(double x, double y) { return fractalNoise(x, y, 0.0, 4.0, 0.0); }
    public static double fractalNoise(double x) { return fractalNoise(x, 0.0, 0.0, 4.0, 0.0); }
    public static Vec3 randomInSphere(double radius) { return libVec(Native.gameValues(206, 0, 0, new double[] {radius}, "")); }
    public static Vec3 randomInSphere() { return randomInSphere(1.0); }
    public static Vec3 randomOnSphere(double radius) { return libVec(Native.gameValues(207, 0, 0, new double[] {radius}, "")); }
    public static Vec3 randomOnSphere() { return randomOnSphere(1.0); }
    public static Vec3 randomInCircle(double radius) { return libVec(Native.gameValues(208, 0, 0, new double[] {radius}, "")); }
    public static Vec3 randomInCircle() { return randomInCircle(1.0); }
    public static Vec3 randomInCone(double axis_x, double axis_y, double axis_z, double degrees) { return libVec(Native.gameValues(209, 0, 0, new double[] {axis_x, axis_y, axis_z, degrees}, "")); }
    public static Vec3 randomInCone(double axis_x, double axis_y, double axis_z) { return randomInCone(axis_x, axis_y, axis_z, 30.0); }
    public static String randomText(double length, String alphabet) { return Native.gameText(210, 0, 0, new double[] {length}, libJoin(alphabet)); }
    public static String randomText(double length) { return randomText(length, "abcdefghijklmnopqrstuvwxyz"); }
    public static String randomText() { return randomText(8.0, "abcdefghijklmnopqrstuvwxyz"); }
    public static void bagAdd(String bag, String item, double copies) { Native.game(211, 0, 0, new double[] {copies}, libJoin(bag, item)); }
    public static void bagAdd(String bag, String item) { bagAdd(bag, item, 1.0); }
    public static String bagNext(String bag) { return Native.gameText(212, 0, 0, new double[] {}, libJoin(bag)); }
    public static String bagPeek(String bag) { return Native.gameText(213, 0, 0, new double[] {}, libJoin(bag)); }
    public static int bagRemaining(String bag) { return (int) Native.game(214, 0, 0, new double[] {}, libJoin(bag)); }
    public static int bagSize(String bag) { return (int) Native.game(215, 0, 0, new double[] {}, libJoin(bag)); }
    public static void bagReset(String bag) { Native.game(216, 0, 0, new double[] {}, libJoin(bag)); }
    public static void bagClear(String bag) { Native.game(217, 0, 0, new double[] {}, libJoin(bag)); }
    public static boolean bagRemove(String bag, String item) { return Native.game(218, 0, 0, new double[] {}, libJoin(bag, item)) != 0.0; }
    public static void bagAvoidRepeats(String bag, boolean on) { Native.game(219, 0, 0, new double[] {on ? 1.0 : 0.0}, libJoin(bag)); }
    public static void bagAvoidRepeats(String bag) { bagAvoidRepeats(bag, true); }
    public static int playSound(String clip, double volume, double pitch) { return (int) Native.game(220, 0, 0, new double[] {volume, pitch}, libJoin(clip)); }
    public static int playSound(String clip, double volume) { return playSound(clip, volume, 1.0); }
    public static int playSound(String clip) { return playSound(clip, 1.0, 1.0); }
    public static int playSoundAt(String clip, double x, double y, double z, double volume, double pitch, double range) { return (int) Native.game(221, 0, 0, new double[] {x, y, z, volume, pitch, range}, libJoin(clip)); }
    public static int playSoundAt(String clip, double x, double y, double z, double volume, double pitch) { return playSoundAt(clip, x, y, z, volume, pitch, 25.0); }
    public static int playSoundAt(String clip, double x, double y, double z, double volume) { return playSoundAt(clip, x, y, z, volume, 1.0, 25.0); }
    public static int playSoundAt(String clip, double x, double y, double z) { return playSoundAt(clip, x, y, z, 1.0, 1.0, 25.0); }
    public static int playSoundOn(Entity entity, String clip, double volume, double pitch, double range) { return (int) Native.game(222, libId(entity), 0, new double[] {volume, pitch, range}, libJoin(clip)); }
    public static int playSoundOn(Entity entity, String clip, double volume, double pitch) { return playSoundOn(entity, clip, volume, pitch, 25.0); }
    public static int playSoundOn(Entity entity, String clip, double volume) { return playSoundOn(entity, clip, volume, 1.0, 25.0); }
    public static int playSoundOn(Entity entity, String clip) { return playSoundOn(entity, clip, 1.0, 1.0, 25.0); }
    public static int loopSound(String clip, double volume) { return (int) Native.game(223, 0, 0, new double[] {volume}, libJoin(clip)); }
    public static int loopSound(String clip) { return loopSound(clip, 1.0); }
    public static int loopSoundOn(Entity entity, String clip, double volume, double range) { return (int) Native.game(224, libId(entity), 0, new double[] {volume, range}, libJoin(clip)); }
    public static int loopSoundOn(Entity entity, String clip, double volume) { return loopSoundOn(entity, clip, volume, 25.0); }
    public static int loopSoundOn(Entity entity, String clip) { return loopSoundOn(entity, clip, 1.0, 25.0); }
    public static void stopSound(double handle) { Native.game(225, 0, 0, new double[] {handle}, ""); }
    public static void stopAllSounds() { Native.game(226, 0, 0, new double[] {}, ""); }
    public static void setSoundVolume(double handle, double volume) { Native.game(227, 0, 0, new double[] {handle, volume}, ""); }
    public static void setSoundPitch(double handle, double pitch) { Native.game(228, 0, 0, new double[] {handle, pitch}, ""); }
    public static void pauseSound(double handle, boolean paused) { Native.game(229, 0, 0, new double[] {handle, paused ? 1.0 : 0.0}, ""); }
    public static void pauseSound(double handle) { pauseSound(handle, true); }
    public static boolean soundPlaying(double handle) { return Native.game(230, 0, 0, new double[] {handle}, "") != 0.0; }
    public static int playSoundSource(Entity entity) { return (int) Native.game(231, libId(entity), 0, new double[] {}, ""); }
    public static void stopSoundSource(Entity entity) { Native.game(232, libId(entity), 0, new double[] {}, ""); }
    public static boolean aiAdd(Entity entity, String brain) { return Native.game(233, libId(entity), 0, new double[] {}, libJoin(brain)) != 0.0; }
    public static boolean aiAdd(Entity entity) { return aiAdd(entity, "guard"); }
    public static boolean aiSetBrain(Entity entity, String brain) { return Native.game(234, libId(entity), 0, new double[] {}, libJoin(brain)) != 0.0; }
    public static boolean aiSetBehavior(Entity entity, String behavior) { return Native.game(235, libId(entity), 0, new double[] {}, libJoin(behavior)) != 0.0; }
    public static boolean aiDefineBehavior(String name, String behavior) { return Native.game(236, 0, 0, new double[] {}, libJoin(name, behavior)) != 0.0; }
    public static String aiBrainBehavior(String brain) { return Native.gameText(237, 0, 0, new double[] {}, libJoin(brain)); }
    public static String aiBehavior(Entity entity) { return Native.gameText(238, libId(entity), 0, new double[] {}, ""); }
    public static boolean aiEnable(Entity entity, boolean on) { return Native.game(239, libId(entity), 0, new double[] {on ? 1.0 : 0.0}, "") != 0.0; }
    public static boolean aiEnable(Entity entity) { return aiEnable(entity, true); }
    public static boolean aiIsAgent(Entity entity) { return Native.game(240, libId(entity), 0, new double[] {}, "") != 0.0; }
    public static boolean aiSetTeam(Entity entity, String team) { return Native.game(241, libId(entity), 0, new double[] {}, libJoin(team)) != 0.0; }
    public static String aiTeam(Entity entity) { return Native.gameText(242, libId(entity), 0, new double[] {}, ""); }
    public static void aiSetRelation(String team_a, String team_b, double relation) { Native.game(243, 0, 0, new double[] {relation}, libJoin(team_a, team_b)); }
    public static int aiRelation(String team_a, String team_b) { return (int) Native.game(244, 0, 0, new double[] {}, libJoin(team_a, team_b)); }
    public static boolean aiHostile(Entity entity, Entity other) { return Native.game(245, libId(entity), libId(other), new double[] {}, "") != 0.0; }
    public static boolean aiMoveTo(Entity entity, double x, double y, double z, boolean run) { return Native.game(246, libId(entity), 0, new double[] {x, y, z, run ? 1.0 : 0.0}, "") != 0.0; }
    public static boolean aiMoveTo(Entity entity, double x, double y, double z) { return aiMoveTo(entity, x, y, z, false); }
    public static void aiStop(Entity entity) { Native.game(247, libId(entity), 0, new double[] {}, ""); }
    public static boolean aiArrived(Entity entity) { return Native.game(248, libId(entity), 0, new double[] {}, "") != 0.0; }
    public static boolean aiIsMoving(Entity entity) { return Native.game(249, libId(entity), 0, new double[] {}, "") != 0.0; }
    public static boolean aiSetTarget(Entity entity, Entity target) { return Native.game(250, libId(entity), libId(target), new double[] {}, "") != 0.0; }
    public static Entity aiTarget(Entity entity) { return new Entity(Native.gameEntity(251, libId(entity), 0, new double[] {}, "")); }
    public static boolean aiCanSee(Entity entity, Entity other) { return Native.game(252, libId(entity), libId(other), new double[] {}, "") != 0.0; }
    public static double aiAwareness(Entity entity, Entity other) { return Native.game(253, libId(entity), libId(other), new double[] {}, ""); }
    public static double aiSuspicion(Entity entity) { return Native.game(254, libId(entity), 0, new double[] {}, ""); }
    public static int aiAlertLevel(Entity entity) { return (int) Native.game(255, libId(entity), 0, new double[] {}, ""); }
    public static String aiState(Entity entity) { return Native.gameText(256, libId(entity), 0, new double[] {}, ""); }
    public static String aiAction(Entity entity) { return Native.gameText(257, libId(entity), 0, new double[] {}, ""); }
    public static String aiEvent(Entity entity) { return Native.gameText(258, libId(entity), 0, new double[] {}, ""); }
    public static String aiPendingAction(Entity entity) { return Native.gameText(259, libId(entity), 0, new double[] {}, ""); }
    public static String aiPendingArgument(Entity entity, double index) { return Native.gameText(260, libId(entity), 0, new double[] {index}, ""); }
    public static String aiPendingArgument(Entity entity) { return aiPendingArgument(entity, 0.0); }
    public static boolean aiFinishAction(Entity entity, boolean success) { return Native.game(261, libId(entity), 0, new double[] {success ? 1.0 : 0.0}, "") != 0.0; }
    public static boolean aiFinishAction(Entity entity) { return aiFinishAction(entity, true); }
    public static void aiSetVisibility(Entity entity, double factor) { Native.game(262, libId(entity), 0, new double[] {factor}, ""); }
    public static void aiMakeNoise(double x, double y, double z, double loudness, String kind) { Native.game(263, 0, 0, new double[] {x, y, z, loudness}, libJoin(kind)); }
    public static void aiMakeNoise(double x, double y, double z, double loudness) { aiMakeNoise(x, y, z, loudness, "noise"); }
    public static void aiMakeNoise(double x, double y, double z) { aiMakeNoise(x, y, z, 15.0, "noise"); }
    public static String aiTeamAlert(String team) { return Native.gameText(264, 0, 0, new double[] {}, libJoin(team)); }
    public static void aiSetTeamAlert(String team, String phase, double x, double y, double z) { Native.game(265, 0, 0, new double[] {x, y, z}, libJoin(team, phase)); }
    public static void aiSetTeamAlert(String team, String phase, double x, double y) { aiSetTeamAlert(team, phase, x, y, 0.0); }
    public static void aiSetTeamAlert(String team, String phase, double x) { aiSetTeamAlert(team, phase, x, 0.0, 0.0); }
    public static void aiSetTeamAlert(String team, String phase) { aiSetTeamAlert(team, phase, 0.0, 0.0, 0.0); }
    public static Vec3[] aiFindPath(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return libVecs(Native.gameValues(266, 0, 0, new double[] {from_x, from_y, from_z, to_x, to_y, to_z}, "")); }
    public static double aiPathLength(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return Native.game(267, 0, 0, new double[] {from_x, from_y, from_z, to_x, to_y, to_z}, ""); }
    public static Vec3[] aiPath(Entity entity) { return libVecs(Native.gameValues(268, libId(entity), 0, new double[] {}, "")); }
    public static Vec3 aiRandomPoint(double x, double y, double z, double radius) { return libVec(Native.gameValues(269, 0, 0, new double[] {x, y, z, radius}, "")); }
    public static Vec3 aiRandomPoint(double x, double y, double z) { return aiRandomPoint(x, y, z, 10.0); }
    public static Vec3 aiSnapToGround(double x, double y, double z) { return libVec(Native.gameValues(270, 0, 0, new double[] {x, y, z}, "")); }
    public static int aiAddDanger(double x, double y, double z, double radius, double amount, double seconds) { return (int) Native.game(271, 0, 0, new double[] {x, y, z, radius, amount, seconds}, ""); }
    public static int aiAddDanger(double x, double y, double z, double radius, double amount) { return aiAddDanger(x, y, z, radius, amount, 0.0); }
    public static int aiAddDanger(double x, double y, double z, double radius) { return aiAddDanger(x, y, z, radius, 5.0, 0.0); }
    public static int aiAddDanger(double x, double y, double z) { return aiAddDanger(x, y, z, 3.0, 5.0, 0.0); }
    public static void aiClearDanger() { Native.game(272, 0, 0, new double[] {}, ""); }
    public static int aiBake(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return (int) Native.game(273, 0, 0, new double[] {from_x, from_y, from_z, to_x, to_y, to_z}, ""); }
    public static double[] aiFindCover(Entity entity, double threat_x, double threat_y, double threat_z, double radius) { return Native.gameValues(274, libId(entity), 0, new double[] {threat_x, threat_y, threat_z, radius}, ""); }
    public static double[] aiFindCover(Entity entity, double threat_x, double threat_y, double threat_z) { return aiFindCover(entity, threat_x, threat_y, threat_z, 12.0); }
    public static boolean aiLineOfSight(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return Native.game(275, 0, 0, new double[] {from_x, from_y, from_z, to_x, to_y, to_z}, "") != 0.0; }
    public static Vec3 aiLastSeen(Entity entity) { return libVec(Native.gameValues(276, libId(entity), 0, new double[] {}, "")); }
    public static void aiSetNumber(Entity entity, String key, double value) { Native.game(277, libId(entity), 0, new double[] {value}, libJoin(key)); }
    public static double aiGetNumber(Entity entity, String key) { return Native.game(278, libId(entity), 0, new double[] {}, libJoin(key)); }
    public static void aiSetText(Entity entity, String key, String value) { Native.game(279, libId(entity), 0, new double[] {}, libJoin(key, value)); }
    public static String aiGetText(Entity entity, String key) { return Native.gameText(280, libId(entity), 0, new double[] {}, libJoin(key)); }
    public static double aiDirectorIntensity() { return Native.game(281, 0, 0, new double[] {}, ""); }
    public static double aiDirectorMenace() { return Native.game(282, 0, 0, new double[] {}, ""); }
    public static String aiDirectorPhase() { return Native.gameText(283, 0, 0, new double[] {}, ""); }
    public static void aiDirectorNote(String habit, double amount) { Native.game(284, 0, 0, new double[] {amount}, libJoin(habit)); }
    public static void aiDirectorNote(String habit) { aiDirectorNote(habit, 1.0); }
    public static double aiDirectorHabit(String habit) { return Native.game(285, 0, 0, new double[] {}, libJoin(habit)); }
    public static void aiSetSpawns(String entity_template, double max_alive, double min_distance, double max_distance, double interval) { Native.game(286, 0, 0, new double[] {max_alive, min_distance, max_distance, interval}, libJoin(entity_template)); }
    public static void aiSetSpawns(String entity_template, double max_alive, double min_distance, double max_distance) { aiSetSpawns(entity_template, max_alive, min_distance, max_distance, 3.0); }
    public static void aiSetSpawns(String entity_template, double max_alive, double min_distance) { aiSetSpawns(entity_template, max_alive, min_distance, 40.0, 3.0); }
    public static void aiSetSpawns(String entity_template, double max_alive) { aiSetSpawns(entity_template, max_alive, 15.0, 40.0, 3.0); }
    public static void aiSetSpawns(String entity_template) { aiSetSpawns(entity_template, 10.0, 15.0, 40.0, 3.0); }
    public static int aiAgentCount() { return (int) Native.game(287, 0, 0, new double[] {}, ""); }
    public static Entity aiMainPlayer() { return new Entity(Native.gameEntity(288, 0, 0, new double[] {}, "")); }
    public static Vec3 aiLobVelocity(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double apex) { return libVec(Native.gameValues(289, 0, 0, new double[] {from_x, from_y, from_z, to_x, to_y, to_z, apex}, "")); }
    public static Vec3 aiLobVelocity(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return aiLobVelocity(from_x, from_y, from_z, to_x, to_y, to_z, 2.0); }
    public static Vec3 aiIntercept(double shooter_x, double shooter_y, double shooter_z, double target_x, double target_y, double target_z, double target_vx, double target_vy, double target_vz, double speed) { return libVec(Native.gameValues(290, 0, 0, new double[] {shooter_x, shooter_y, shooter_z, target_x, target_y, target_z, target_vx, target_vy, target_vz, speed}, "")); }
    public static int voxelDefine(String name, boolean solid, boolean climbable, boolean liquid, double cost, double danger, boolean door) { return (int) Native.game(291, 0, 0, new double[] {solid ? 1.0 : 0.0, climbable ? 1.0 : 0.0, liquid ? 1.0 : 0.0, cost, danger, door ? 1.0 : 0.0}, libJoin(name)); }
    public static int voxelDefine(String name, boolean solid, boolean climbable, boolean liquid, double cost, double danger) { return voxelDefine(name, solid, climbable, liquid, cost, danger, false); }
    public static int voxelDefine(String name, boolean solid, boolean climbable, boolean liquid, double cost) { return voxelDefine(name, solid, climbable, liquid, cost, 0.0, false); }
    public static int voxelDefine(String name, boolean solid, boolean climbable, boolean liquid) { return voxelDefine(name, solid, climbable, liquid, 1.0, 0.0, false); }
    public static int voxelDefine(String name, boolean solid, boolean climbable) { return voxelDefine(name, solid, climbable, false, 1.0, 0.0, false); }
    public static int voxelDefine(String name, boolean solid) { return voxelDefine(name, solid, false, false, 1.0, 0.0, false); }
    public static int voxelDefine(String name) { return voxelDefine(name, true, false, false, 1.0, 0.0, false); }
    public static void voxelSet(double x, double y, double z, String type) { Native.game(292, 0, 0, new double[] {x, y, z}, libJoin(type)); }
    public static void voxelFill(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, String type) { Native.game(293, 0, 0, new double[] {from_x, from_y, from_z, to_x, to_y, to_z}, libJoin(type)); }
    public static String voxelGet(double x, double y, double z) { return Native.gameText(294, 0, 0, new double[] {x, y, z}, ""); }
    public static void voxelClear() { Native.game(295, 0, 0, new double[] {}, ""); }
    public static void voxelSolidBelow(double y) { Native.game(296, 0, 0, new double[] {y}, ""); }
    public static Vec3[] voxelPath(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double height, double step, double drop, double jump, double danger_weight) { return libVecs(Native.gameValues(297, 0, 0, new double[] {from_x, from_y, from_z, to_x, to_y, to_z, height, step, drop, jump, danger_weight}, "")); }
    public static Vec3[] voxelPath(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double height, double step, double drop, double jump) { return voxelPath(from_x, from_y, from_z, to_x, to_y, to_z, height, step, drop, jump, 10.0); }
    public static Vec3[] voxelPath(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double height, double step, double drop) { return voxelPath(from_x, from_y, from_z, to_x, to_y, to_z, height, step, drop, 0.0, 10.0); }
    public static Vec3[] voxelPath(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double height, double step) { return voxelPath(from_x, from_y, from_z, to_x, to_y, to_z, height, step, 3.0, 0.0, 10.0); }
    public static Vec3[] voxelPath(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z, double height) { return voxelPath(from_x, from_y, from_z, to_x, to_y, to_z, height, 1.0, 3.0, 0.0, 10.0); }
    public static Vec3[] voxelPath(double from_x, double from_y, double from_z, double to_x, double to_y, double to_z) { return voxelPath(from_x, from_y, from_z, to_x, to_y, to_z, 2.0, 1.0, 3.0, 0.0, 10.0); }
    public static Vec3[] voxelFlee(double from_x, double from_y, double from_z, double threat_x, double threat_y, double threat_z, double distance, double height) { return libVecs(Native.gameValues(298, 0, 0, new double[] {from_x, from_y, from_z, threat_x, threat_y, threat_z, distance, height}, "")); }
    public static Vec3[] voxelFlee(double from_x, double from_y, double from_z, double threat_x, double threat_y, double threat_z, double distance) { return voxelFlee(from_x, from_y, from_z, threat_x, threat_y, threat_z, distance, 2.0); }
    public static Vec3[] voxelFlee(double from_x, double from_y, double from_z, double threat_x, double threat_y, double threat_z) { return voxelFlee(from_x, from_y, from_z, threat_x, threat_y, threat_z, 10.0, 2.0); }
    public static void goapAction(String planner, String name, String needs, String gives, double cost) { Native.game(299, 0, 0, new double[] {cost}, libJoin(planner, name, needs, gives)); }
    public static void goapAction(String planner, String name, String needs, String gives) { goapAction(planner, name, needs, gives, 1.0); }
    public static void goapRemove(String planner, String name) { Native.game(300, 0, 0, new double[] {}, libJoin(planner, name)); }
    public static void goapClear(String planner) { Native.game(301, 0, 0, new double[] {}, libJoin(planner)); }
    public static String goapPlan(String planner, String start, String goal) { return Native.gameText(302, 0, 0, new double[] {}, libJoin(planner, start, goal)); }
    public static int chessNew(String fen) { return (int) Native.game(303, 0, 0, new double[] {}, libJoin(fen)); }
    public static int chessNew() { return chessNew(""); }
    public static void chessFree(double game) { Native.game(304, 0, 0, new double[] {game}, ""); }
    public static boolean chessLoad(double game, String fen) { return Native.game(305, 0, 0, new double[] {game}, libJoin(fen)) != 0.0; }
    public static String chessFen(double game) { return Native.gameText(306, 0, 0, new double[] {game}, ""); }
    public static boolean chessMove(double game, String move) { return Native.game(307, 0, 0, new double[] {game}, libJoin(move)) != 0.0; }
    public static boolean chessIsLegal(double game, String move) { return Native.game(308, 0, 0, new double[] {game}, libJoin(move)) != 0.0; }
    public static boolean chessUndo(double game) { return Native.game(309, 0, 0, new double[] {game}, "") != 0.0; }
    public static String chessBestMove(double game, double level) { return Native.gameText(310, 0, 0, new double[] {game, level}, ""); }
    public static String chessBestMove(double game) { return chessBestMove(game, 5.0); }
    public static String chessStatus(double game) { return Native.gameText(311, 0, 0, new double[] {game}, ""); }
    public static int chessTurn(double game) { return (int) Native.game(312, 0, 0, new double[] {game}, ""); }
    public static String chessMoves(double game) { return Native.gameText(313, 0, 0, new double[] {game}, ""); }
    public static int chessPiece(double game, String square) { return (int) Native.game(314, 0, 0, new double[] {game}, libJoin(square)); }
    public static String chessBoard(double game) { return Native.gameText(315, 0, 0, new double[] {game}, ""); }
    public static String chessHistory(double game) { return Native.gameText(316, 0, 0, new double[] {game}, ""); }
    public static boolean nnCreate(String name, String layers, String hidden, String output) { return Native.game(317, 0, 0, new double[] {}, libJoin(name, layers, hidden, output)) != 0.0; }
    public static boolean nnCreate(String name, String layers, String hidden) { return nnCreate(name, layers, hidden, "sigmoid"); }
    public static boolean nnCreate(String name, String layers) { return nnCreate(name, layers, "tanh", "sigmoid"); }
    public static double[] nnRun(String name, double... inputs) { return Native.gameValues(318, 0, 0, libCat(new double[] {}, inputs), libJoin(name)); }
    public static double nnTrain(String name, double rate, double... inputs_then_targets) { return Native.game(319, 0, 0, libCat(new double[] {rate}, inputs_then_targets), libJoin(name)); }
    public static String nnSave(String name) { return Native.gameText(320, 0, 0, new double[] {}, libJoin(name)); }
    public static boolean nnLoad(String name, String data) { return Native.game(321, 0, 0, new double[] {}, libJoin(name, data)) != 0.0; }
    public static int qChoose(String name, String state, double actions) { return (int) Native.game(322, 0, 0, new double[] {actions}, libJoin(name, state)); }
    public static int qChoose(String name, String state) { return qChoose(name, state, 4.0); }
    public static int qBest(String name, String state) { return (int) Native.game(323, 0, 0, new double[] {}, libJoin(name, state)); }
    public static void qLearn(String name, String state, double action, double reward, String next, boolean terminal) { Native.game(324, 0, 0, new double[] {action, reward, terminal ? 1.0 : 0.0}, libJoin(name, state, next)); }
    public static void qLearn(String name, String state, double action, double reward, String next) { qLearn(name, state, action, reward, next, false); }
    public static void qSettings(String name, double epsilon, double alpha, double gamma) { Native.game(325, 0, 0, new double[] {epsilon, alpha, gamma}, libJoin(name)); }
    public static void qSettings(String name, double epsilon, double alpha) { qSettings(name, epsilon, alpha, 0.9); }
    public static void qSettings(String name, double epsilon) { qSettings(name, epsilon, 0.2, 0.9); }
    public static void qSettings(String name) { qSettings(name, 0.1, 0.2, 0.9); }
    public static String qSave(String name) { return Native.gameText(326, 0, 0, new double[] {}, libJoin(name)); }
    public static boolean qLoad(String name, String data) { return Native.game(327, 0, 0, new double[] {}, libJoin(name, data)) != 0.0; }
    public static void markovTrain(String name, String text, double order, boolean letters) { Native.game(328, 0, 0, new double[] {order, letters ? 1.0 : 0.0}, libJoin(name, text)); }
    public static void markovTrain(String name, String text, double order) { markovTrain(name, text, order, false); }
    public static void markovTrain(String name, String text) { markovTrain(name, text, 1.0, false); }
    public static String markovGenerate(String name, double max_length, boolean letters) { return Native.gameText(329, 0, 0, new double[] {max_length, letters ? 1.0 : 0.0}, libJoin(name)); }
    public static String markovGenerate(String name, double max_length) { return markovGenerate(name, max_length, false); }
    public static String markovGenerate(String name) { return markovGenerate(name, 20.0, false); }
    public static double randomRange(double low, double high) { return Native.game(330, 0, 0, new double[] {low, high}, ""); }
    public static double randomRange(double low) { return randomRange(low, 1.0); }
    public static double randomRange() { return randomRange(0.0, 1.0); }
    public static int randomInteger(double low, double high) { return (int) Native.game(331, 0, 0, new double[] {low, high}, ""); }
    public static int randomInteger(double low) { return randomInteger(low, 10.0); }
    public static int randomInteger() { return randomInteger(0.0, 10.0); }
    public static String randomPick(String choices) { return Native.gameText(332, 0, 0, new double[] {}, libJoin(choices)); }
    public static double randomWhole(double low, double high) { return Native.game(333, 0, 0, new double[] {low, high}, ""); }
    public static double randomWhole(double low) { return randomWhole(low, 6.0); }
    public static double randomWhole() { return randomWhole(1.0, 6.0); }
    public static double bagCount(String bag) { return Native.game(334, 0, 0, new double[] {}, libJoin(bag)); }
    public static void aiMakeNoiseFrom(Entity entity, double loudness, String kind) { Native.game(335, libId(entity), 0, new double[] {loudness}, libJoin(kind)); }
    public static void aiMakeNoiseFrom(Entity entity, double loudness) { aiMakeNoiseFrom(entity, loudness, "noise"); }
    public static void aiMakeNoiseFrom(Entity entity) { aiMakeNoiseFrom(entity, 15.0, "noise"); }
    public static double secureRandom(double low, double high) { return Native.game(336, 0, 0, new double[] {low, high}, ""); }
    public static double secureRandom(double low) { return secureRandom(low, 1.0); }
    public static double secureRandom() { return secureRandom(0.0, 1.0); }
    public static double secureRandomInteger(double low, double high) { return Native.game(337, 0, 0, new double[] {low, high}, ""); }
    public static double secureRandomInteger(double low) { return secureRandomInteger(low, 100.0); }
    public static double secureRandomInteger() { return secureRandomInteger(0.0, 100.0); }
    public static boolean secureChance(double probability) { return Native.game(338, 0, 0, new double[] {probability}, "") != 0.0; }
    public static boolean secureChance() { return secureChance(0.5); }
    public static String securePick(String choices) { return Native.gameText(339, 0, 0, new double[] {}, libJoin(choices)); }
    public static String secureRandomText(double length, String alphabet) { return Native.gameText(340, 0, 0, new double[] {length}, libJoin(alphabet)); }
    public static String secureRandomText(double length) { return secureRandomText(length, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"); }
    public static String secureRandomText() { return secureRandomText(16.0, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"); }
    public static String secureToken(double byte_count) { return Native.gameText(341, 0, 0, new double[] {byte_count}, ""); }
    public static String secureToken() { return secureToken(32.0); }
    public static String secureHex(double byte_count) { return Native.gameText(342, 0, 0, new double[] {byte_count}, ""); }
    public static String secureHex() { return secureHex(16.0); }
    public static String secureUuid() { return Native.gameText(343, 0, 0, new double[] {}, ""); }
}

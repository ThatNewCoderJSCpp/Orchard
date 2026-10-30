package orchard;

public abstract class Script {
    protected Entity entity = Entity.NONE;

    public Entity entity() { return entity; }

    public void onStart() {
    }

    public void onTick(double dt) {
    }

    public void onStop() {
    }
}

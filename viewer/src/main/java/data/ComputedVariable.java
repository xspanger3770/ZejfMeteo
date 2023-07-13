package data;

import java.util.UUID;

public class ComputedVariable {

    private final int samplesPerHour;

    private final UUID uuid;

    private final ComputedLog[] computedLogs;

    public ComputedVariable(int samplesPerHour, UUID calculationUUID){
        this.samplesPerHour = samplesPerHour;
        this.uuid = calculationUUID;

        computedLogs = new ComputedLog[samplesPerHour];
        for(int i = 0; i < samplesPerHour; i++){
            computedLogs[i] = new ComputedLog();
        }
    }

    public UUID getUuid() {
        return uuid;
    }

    public ComputedLog[] getComputedLogs() {
        return computedLogs;
    }

    public int getSamplesPerHour() {
        return samplesPerHour;
    }
}

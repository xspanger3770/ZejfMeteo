package data.computation;

import data.DataHour;
import data.DataManager;

import java.io.Serializable;
import java.util.Objects;
import java.util.UUID;

public class VariableComputation implements Serializable {

    private final int variableId;
    private final int samplesPerHour;

    private final String displayName;

    private final String units;

    private final ComputationMode computationMode;

    private final UUID uuid;

    public VariableComputation(int variableId, int samplesPerHour, String displayName, String units, ComputationMode computationMode) {
        this.variableId = variableId;
        this.samplesPerHour = samplesPerHour;
        this.displayName = displayName;
        this.units = units;
        this.computationMode = computationMode;
        this.uuid = UUID.randomUUID();
    }

    public int getVariableId() {
        return variableId;
    }

    public int getSamplesPerHour() {
        return samplesPerHour;
    }

    public String getDisplayName() {
        return displayName;
    }

    public String getUnits() {
        return units;
    }

    public ComputationMode getCalculationMode() {
        return computationMode;
    }

    public UUID getUuid() {
        return uuid;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        VariableComputation that = (VariableComputation) o;
        return variableId == that.variableId && samplesPerHour == that.samplesPerHour && Objects.equals(displayName, that.displayName) && Objects.equals(units, that.units) && Objects.equals(computationMode, that.computationMode);
    }

    @Override
    public int hashCode() {
        return Objects.hash(variableId, samplesPerHour, displayName, units, computationMode);
    }

    public boolean calculate(DataManager dataManager, VariableComputation calculation, DataHour dataHour, ComputedVariable computedVariable) {
        return getCalculationMode().calculateValue(dataManager, calculation, dataHour, computedVariable);
    }

    public ComputationMode getComputationMode() {
        return computationMode;
    }

}

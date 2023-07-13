package data;

import java.io.Serializable;
import java.util.Objects;
import java.util.UUID;

public class VariableCalculation implements Serializable {

    private final int variableId;
    private final int samplesPerHour;

    private final String displayName;

    private final String units;

    private final CalculationMode calculationMode;

    private final UUID uuid;

    public VariableCalculation(int variableId, int samplesPerHour, String displayName, String units, CalculationMode calculationMode) {
        this.variableId = variableId;
        this.samplesPerHour = samplesPerHour;
        this.displayName = displayName;
        this.units = units;
        this.calculationMode = calculationMode;
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

    public CalculationMode getCalculationMode() {
        return calculationMode;
    }

    public UUID getUuid() {
        return uuid;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        VariableCalculation that = (VariableCalculation) o;
        return variableId == that.variableId && samplesPerHour == that.samplesPerHour && Objects.equals(displayName, that.displayName) && Objects.equals(units, that.units) && Objects.equals(calculationMode, that.calculationMode);
    }

    @Override
    public int hashCode() {
        return Objects.hash(variableId, samplesPerHour, displayName, units, calculationMode);
    }
}

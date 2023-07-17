package data.computation;

import data.DataHour;
import data.DataManager;

import java.io.Serializable;

public abstract class ComputationMode implements Serializable {

    public abstract boolean calculateValue(DataManager dataManager, VariableComputation calculation, DataHour dataHour, ComputedVariable computedVariable);
}

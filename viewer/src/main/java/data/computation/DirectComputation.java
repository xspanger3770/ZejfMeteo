package data.computation;

import data.DataHour;
import data.DataManager;
import data.DataVariable;

public class DirectComputation extends ComputationMode {
    @Override
    public boolean calculateValue(DataManager dataManager, VariableComputation computation, DataHour dataHour, ComputedVariable computedVariable) {
        if(dataHour == null){
            return false;
        }

        DataVariable dataVariable = dataHour.getVariable(computation.getVariableId(), computation.getSamplesPerHour(), false);
        if(dataVariable == null){
            return false;
        }

        boolean result = false;

        for(int sample = 0; sample < computation.getSamplesPerHour(); sample++){
            ComputedLog log = computedVariable.getComputedLogs()[sample];

            if(log.getStatus() == ComputationStatus.FINISHED){
                continue;
            }

            double val = dataVariable.getData()[sample];

            if(val == DataManager.VALUE_EMPTY){
                continue;
            } else if(val == DataManager.VALUE_NOT_MEASURED){
                log.setStatus(ComputationStatus.NOT_MEASURED);
                continue;
            }

            log.setValue(val);
            log.setStatus(ComputationStatus.FINISHED);
            computedVariable.setLastLog(sample);
            result = true;
            System.out.println("COMPUTEDED "+sample+"@"+computation.getVariableId());
        }

        return result;
    }
}

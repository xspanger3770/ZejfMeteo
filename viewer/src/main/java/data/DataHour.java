package data;

import java.io.Serializable;
import java.util.LinkedList;
import java.util.List;

public class DataHour implements Serializable {

    private int hourId;

    private List<DataVariable> variables;

    public DataHour(int hourId){
        this.hourId = hourId;
        variables = new LinkedList<>();
    }

    public int getHourId() {
        return hourId;
    }

    public List<DataVariable> getVariables() {
        return variables;
    }
}

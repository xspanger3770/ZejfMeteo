package data;

import java.io.Serializable;
import java.util.LinkedList;
import java.util.List;

public class DataHour implements Serializable {

    private long hourNumber;

    private transient boolean modified = false;

    private List<DataVariable> variablesRaw;
    private List<DataVariable> variablesComputed;

    public DataHour(long hourNumber){
        this.hourNumber = hourNumber;
        variablesRaw = new LinkedList<>();
        variablesComputed = new LinkedList<>();
    }

    public long getHourNumber() {
        return hourNumber;
    }

    public boolean isModified() {
        return modified;
    }
}

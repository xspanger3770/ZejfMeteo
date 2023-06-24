package data;

import java.io.Serializable;
import java.util.Arrays;

public class DataVariable implements Serializable {

    private final int id;
    private final int samplesPerHour;

    private final double[] data;

    public DataVariable(int id, int samplesPerHour){
        this.id = id;
        this.samplesPerHour = samplesPerHour;
        this.data = new double[samplesPerHour];
        Arrays.fill(data, DataManager.VAL_ERROR);
    }

    public int getId() {
        return id;
    }

    public int getSamplesPerHour() {
        return samplesPerHour;
    }

    public double[] getData() {
        return data;
    }
}

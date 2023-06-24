package data;

import main.ZejfMeteo;

import java.io.File;
import java.util.LinkedList;
import java.util.List;

public class DataManager {

    public static final int PERMANENTLY_LOADED_HOURS = 24;
    public static final double VAL_ERROR = -999.0;
    public static final double VAL_NOT_MEASURED = -998.0;
    private final List<DataHour> dataHours;

    public static final File DATA_FOLDER = new File(ZejfMeteo.MAIN_FOLDER, "/data/");

    public DataManager(){
        dataHours = new LinkedList<>();
    }

    public void load(){

    }

}

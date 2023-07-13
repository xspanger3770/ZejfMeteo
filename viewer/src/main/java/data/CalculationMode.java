package data;

import java.io.Serializable;

public abstract class CalculationMode implements Serializable {

    public abstract double calculateValue(int hourNumber, int sampleNumber);

}

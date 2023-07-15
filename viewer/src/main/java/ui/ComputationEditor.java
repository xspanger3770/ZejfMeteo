package ui;

import data.computation.VariableComputation;
import exception.RuntimeApplicationException;
import main.ZejfMeteo;

import javax.swing.*;
import java.awt.*;

public class ComputationEditor extends JDialog {

        private final VariableComputation variableComputation;

        private final JTextField variableIdField;
        private final JTextField samplesPerHourField;
        private final JTextField displayNameField;
        private final JTextField unitsField;

        public ComputationEditor(Frame parent, VariableComputation variableComputation) {
            super(parent, true);
            this.variableComputation = variableComputation;

            setTitle("Variable Modification");
            setSize(300, 200);
            setLocationRelativeTo(null);
            setLayout(new GridLayout(5, 2));

            JLabel variableIdLabel = new JLabel("Variable ID:");
            variableIdField = new JTextField(String.valueOf(variableComputation.getVariableId()));
            JLabel samplesPerHourLabel = new JLabel("Samples per Hour:");
            samplesPerHourField = new JTextField(String.valueOf(variableComputation.getSamplesPerHour()));
            JLabel displayNameLabel = new JLabel("Display Name:");
            displayNameField = new JTextField(variableComputation.getDisplayName());
            JLabel unitsLabel = new JLabel("Units:");
            unitsField = new JTextField(variableComputation.getUnits());

            JButton saveButton = new JButton("Save");
            saveButton.addActionListener(e -> {
                try {
                    saveChanges();
                    ComputationEditor.super.dispose();
                } catch(Exception ex) {
                    ZejfMeteo.handleException(new RuntimeApplicationException(ex.getMessage()));
                }
            });

            JButton cancelButton = new JButton("Cancel");
            cancelButton.addActionListener(e -> ComputationEditor.super.dispose());

            add(variableIdLabel);
            add(variableIdField);
            add(samplesPerHourLabel);
            add(samplesPerHourField);
            add(displayNameLabel);
            add(displayNameField);
            add(unitsLabel);
            add(unitsField);
            add(saveButton);
            add(cancelButton);

            setVisible(true);
        }

        private void saveChanges() {
            int variableId = Integer.parseInt(variableIdField.getText());
            int samplesPerHour = Integer.parseInt(samplesPerHourField.getText());
            String displayName = displayNameField.getText();
            String units = unitsField.getText();

            ZejfMeteo.getDataManager().getVariableCalculations().remove(variableComputation);
            ZejfMeteo.getDataManager().getVariableCalculations().add(new VariableComputation(variableId, samplesPerHour, displayName, units, variableComputation.getComputationMode()));

        }
    }

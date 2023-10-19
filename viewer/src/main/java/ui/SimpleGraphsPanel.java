package ui;

import data.computation.VariableComputation;
import main.ZejfMeteo;

import javax.swing.*;
import javax.swing.event.DocumentEvent;
import javax.swing.event.DocumentListener;
import java.awt.*;
import java.awt.event.MouseWheelEvent;
import java.awt.event.MouseWheelListener;
import java.util.Timer;
import java.util.TimerTask;

public class SimpleGraphsPanel extends JPanel {

    private JComboBox<VariableComputation> variableSelector;

    public SimpleGraphsPanel(){
        setLayout(new BorderLayout());
        GridLayout gridLayout = new GridLayout(2,2);
        gridLayout.setHgap(5);
        gridLayout.setVgap(5);
        JPanel configPanel = new JPanel(gridLayout);
        configPanel.add(new JLabel("Variable: "));
        configPanel.add(createSelector());
        configPanel.add(new JLabel("Duration (minutes):"));

        var spinnerModel = new SpinnerNumberModel();
        spinnerModel.setMinimum(1);
        spinnerModel.setMaximum(60 * 24 * 7);
        spinnerModel.setValue(60);

        JSpinner durationSpinner = new JSpinner(spinnerModel);

        JComponent comp = durationSpinner.getEditor();
        JFormattedTextField field = (JFormattedTextField) comp.getComponent(0);
        configPanel.add(durationSpinner);

        JPanel strut = new JPanel();
        strut.add(configPanel);

        add(strut, BorderLayout.PAGE_START);

        JPanel graphPanel = new GraphCanvasPanel(this){
            @Override
            public int getDurationMinutes() {
                return (int) spinnerModel.getValue();
            }
        };

        field.getDocument().addDocumentListener(new DocumentListener() {
            @Override
            public void insertUpdate(DocumentEvent documentEvent) {
                graphPanel.repaint();
            }

            @Override
            public void removeUpdate(DocumentEvent documentEvent) {
            }

            @Override
            public void changedUpdate(DocumentEvent documentEvent) {
            }
        });

        graphPanel.addMouseWheelListener(mouseWheelEvent -> {
            int val = (int)spinnerModel.getValue();
            int diff = Math.max(1, (int) (val * 0.1));
            int nv = val + diff * (mouseWheelEvent.getWheelRotation() > 0 ? 1 : -1);
            int newValue = Math.max(1, Math.min(60 * 24 * 7, nv));
            spinnerModel.setValue(newValue);
        });

        add(graphPanel, BorderLayout.CENTER);

        java.util.Timer timer = new Timer();

        TimerTask task = new TimerTask() {

            int counter = 0;

            @Override
            public void run() {
                counter ++;
                if(counter >= (int) (spinnerModel.getValue()) / 60) {
                    SimpleGraphsPanel.super.repaint();
                    counter = 0;
                }
            }
        };

        // Schedule the task to run every two minutes, starting from now
        timer.schedule(task, 1000, 1000);
    }

    public VariableComputation getSelectedVariable(){
        return (VariableComputation) variableSelector.getSelectedItem();
    }

    private JComboBox<VariableComputation> createSelector() {
        if(variableSelector == null) {
            variableSelector = new JComboBox<>();
            variableSelector.setRenderer(new ItemListRenderer());
        }

        variableSelector.addItemListener(itemEvent -> SimpleGraphsPanel.super.repaint());

        updateSelector();

        return variableSelector;
    }

    private void updateSelector() {
        variableSelector.removeAllItems();
        for(VariableComputation variableComputation : ZejfMeteo.getDataManager().getVariableCalculations()){
            variableSelector.addItem(variableComputation);
        }
    }

    private static class ItemListRenderer extends DefaultListCellRenderer {
        @Override
        public Component getListCellRendererComponent(JList<?> list, Object value, int index,
                                                      boolean isSelected, boolean cellHasFocus) {
            super.getListCellRendererComponent(list, value, index, isSelected, cellHasFocus);
            if (value instanceof VariableComputation item) {
                setText(item.getDisplayName());
            }
            return this;
        }
    }


}

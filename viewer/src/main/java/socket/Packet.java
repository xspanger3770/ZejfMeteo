package socket;

public record Packet(int from, int to, int ttl, int tx_id, int command, int checksum, int message_size,
                     String message) {

    public static final int CHECKSUM_BYPASS_VALUE = 0;

    public static Packet fromString(String str) {
        if (!str.startsWith("{") || !str.endsWith("}")) {
            return null;
        }

        String sub = str.substring(1, str.length() - 1);
        String[] data = sub.split(";");

        if (data.length < 7 || data.length > 8) {
            return null;
        }

        int from, to, ttl, tx_id, command, checksum, message_size;
        String message = "";


        try {
            from = Integer.parseInt(data[0]);
            to = Integer.parseInt(data[1]);
            ttl = Integer.parseInt(data[2]);
            tx_id = Integer.parseInt(data[3]);
            command = Integer.parseInt(data[4]);
            checksum = Integer.parseInt(data[5]);
            message_size = Integer.parseInt(data[6]);
            if (data.length == 8) {
                message = data[7];
            }
        } catch (Exception e) {
            return null;
        }

        return new Packet(from, to, ttl, tx_id, command, checksum, message_size, message);
    }

    @Override
    public String toString() {
        return String.format("{%d;%d;%d;%d;%d;%d;%d;%s}\n", from, to, ttl, tx_id, command, CHECKSUM_BYPASS_VALUE, message_size, message);
    }

}

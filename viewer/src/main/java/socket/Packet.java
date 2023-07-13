package socket;

public record Packet(int from, int to, int ttl, long flags, int command, long checksum, int message_size,
                     String message) {

    public static final long CHECKSUM_BYPASS_VALUE = 0;

    public static Packet fromString(String str) {
        if (!str.startsWith("{") || !str.endsWith("}")) {
            System.err.println("ERR {}");
            return null;
        }

        String sub = str.substring(1, str.length() - 1);
        String[] data = sub.split(";");

        if (data.length < 7 || data.length > 8) {
            System.out.println("ERR count "+data.length);
            return null;
        }

        int from, to, ttl, command, message_size;
        long flags, checksum;
        String message = "";


        try {
            from = Integer.parseInt(data[0]);
            to = Integer.parseInt(data[1]);
            command = Integer.parseInt(data[2]);
            ttl = Integer.parseInt(data[3]);
            checksum = Long.parseLong(data[4]);
            flags = Long.parseLong(data[5]);
            message_size = Integer.parseInt(data[6]);
            if (data.length == 8) {
                message = data[7];
            }
        } catch (Exception e) {
            System.out.println("ERR "+e.getMessage());
            return null;
        }

        return new Packet(from, to, ttl, flags, command, checksum, message_size, message);
    }

    @Override
    public String toString() {
        return String.format("{%d;%d;%d;%d;%d;%d;%d;%s}\n", from, to, command, ttl, CHECKSUM_BYPASS_VALUE, flags, message_size, message);
    }

}

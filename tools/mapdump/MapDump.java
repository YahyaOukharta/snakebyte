import com.codingame.game.grid.*;
import java.security.SecureRandom;
import java.util.*;

// Prints the first position of a game for each seed given: the official generator, seeded as the CodinGame SDK does.
public class MapDump {
    public static void main(String[] a) throws Exception {
        for (String s : a) {
            long seed = Long.parseLong(s);
            SecureRandom r = SecureRandom.getInstance("SHA1PRNG");
            r.setSeed(seed);
            Grid g = new GridMaker(r, 4).make();
            List<List<Coord>> spawns = g.detectSpawnIslands().stream().map(x -> x.stream().sorted().toList()).toList();
            List<List<Coord>> birds = new ArrayList<>();
            for (int p = 0; p < 2; p++)
                for (List<Coord> sp : spawns) {
                    List<Coord> body = new ArrayList<>();
                    for (Coord c : sp) {
                        if (p == 1) c = g.opposite(c);
                        body.add(c);
                        if (body.size() == 1) {
                            Tile left = g.get(c.add(-1, 0)), right = g.get(c.add(1, 0));
                            if (left.getType() == Tile.TYPE_WALL && right.getType() == Tile.TYPE_WALL) {
                                g.get(c.add(-1, 0)).clear();
                                g.get(g.opposite(c.add(-1, 0))).clear();
                            }
                        }
                    }
                    birds.add(body);
                }
            StringBuilder o = new StringBuilder();
            o.append("SEED ").append(seed).append("\n").append(g.width).append(" ").append(g.height).append("\n");
            for (int y = 0; y < g.height; y++) {
                for (int x = 0; x < g.width; x++) o.append(g.get(x, y).getType() == Tile.TYPE_WALL ? '#' : '.');
                o.append("\n");
            }
            o.append(g.apples.size()).append("\n");
            for (Coord c : g.apples) o.append(c.getX()).append(" ").append(c.getY()).append("\n");
            o.append(birds.size()).append("\n");
            for (List<Coord> b : birds) {
                for (int i = 0; i < b.size(); i++) o.append(i > 0 ? " " : "").append(b.get(i).getX()).append(" ").append(b.get(i).getY());
                o.append("\n");
            }
            System.out.print(o);
        }
    }
}

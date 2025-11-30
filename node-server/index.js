//// ESM only
import chalk from 'chalk'; // highlighting the terminal
import express from 'express'
import cors from 'cors'
import Database from 'better-sqlite3';
import moment from 'moment'
import { WebSocketServer } from 'ws';

const app = express();
const PORT = 3000;

app.use(cors());
app.use(express.json());

const wss = new WebSocketServer({ port: 3001 });

wss.on('connection', ws => {
    ws.send(JSON.stringify({ event: "serverOnline" }));
});

// Connect to SQLite database
const db = new Database('users.db');

// Create table if it doesn't exist ---> ID primary key gets increased automatically + deleted IDs are never reused
// NB: db.prepare() is sync (sync statement).
// NB: the run() method of a sync statement is also sync
db.prepare(`
  CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY,
    name TEXT,
    age INT
  )
`).run();


// GET all users 
app.get('/api/users', (req, res) => {
  const users = db.prepare('SELECT * FROM users ').all();
  res.json(users);
});

// POST a new item
app.post('/api/users', (req, res) => {
  const { name, age } = req.body;
  const stmt = db.prepare('INSERT INTO users  (name, age) VALUES (?, ?)');
  const info = stmt.run(name, age);
  const newUser = { id: info.lastInsertRowid, name, age };
  res.status(201).json(newUser);
});


// PUT: update item
app.put('/api/users/:id', (req, res) => {
  const id = req.params.id;
  const {name, age } = req.body;
  db.prepare('UPDATE users SET name = ?, age = ? WHERE id = ?').run(name, age, id);
  res.json({ id, name, age });
});


// DELETE an item by ID
app.delete('/api/users/:id', (req, res) => {
  const id = req.params.id;
  db.prepare('DELETE FROM users WHERE id = ?').run(id);
  res.status(204).send();
});


app.listen(PORT, () => {
  const now = moment()
  console.log(chalk.cyan(`Server running on http://localhost:${PORT} at timestamp ${now.toLocaleString()}`));
});

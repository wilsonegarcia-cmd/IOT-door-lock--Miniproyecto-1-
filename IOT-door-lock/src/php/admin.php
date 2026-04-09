<?php
session_start();

$app_timezone_name = getenv('APP_TIMEZONE') ?: 'America/Guayaquil';
date_default_timezone_set($app_timezone_name);

$utc_timezone = new DateTimeZone('UTC');
$app_timezone = new DateTimeZone($app_timezone_name);

function format_utc_timestamp(string $timestamp, DateTimeZone $utc_timezone, DateTimeZone $app_timezone): string
{
    $date_time = DateTime::createFromFormat('Y-m-d H:i:s', $timestamp, $utc_timezone);

    if ($date_time === false) {
        return $timestamp;
    }

    $date_time->setTimezone($app_timezone);
    return $date_time->format('Y-m-d H:i:s');
}

$db_host = getenv('DB_HOST') ?: 'localhost';
$db_user = getenv('DB_USER') ?: 'root';
$db_pass = getenv('DB_PASS') ?: '';
$db_name = getenv('DB_NAME') ?: 'rfid_access';
$db = new mysqli($db_host, $db_user, $db_pass, $db_name);

if ($db->connect_error) {
    die("Error de conexion MySQL: " . $db->connect_error);
}

$db->query("SET time_zone = '+00:00'");

// Gestion de exportaciones CSV
if (isset($_GET['export'])) {
    header('Content-Type: text/csv');
    header('Content-Disposition: attachment; filename="logs_acces_'.date('Y-m-d').'.csv"');
    
    $output = fopen('php://output', 'w');
    fputcsv($output, ['ID', 'UID de credencial', 'Estado', 'Fecha y hora'], ';');
    
    $logs = $db->query("SELECT * FROM logs ORDER BY timestamp DESC");
    while ($row = $logs->fetch_assoc()) {
        fputcsv($output, [
            $row['id'],
            $row['uid'],
            $row['access_status'] === 'GRANTED' ? 'Permitido' : 'Denegado',
            format_utc_timestamp($row['timestamp'], $utc_timezone, $app_timezone)
        ], ';');
    }
    fclose($output);
    exit;
}

// Autenticacion de administrador
if (isset($_POST['login'])) {
    if ($_POST['username'] === 'admin' && $_POST['password'] === 'admin123') {
        $_SESSION['loggedin'] = true;
    } else {
        $error = "Credenciales incorrectas";
    }
}

// Cerrar sesion
if (isset($_GET['logout'])) {
    session_destroy();
    header("Location: admin.php");
    exit;
}

// Agregar credencial
if (isset($_POST['add_badge'])) {
    $uid = $db->real_escape_string($_POST['new_uid']);
    $name = $db->real_escape_string($_POST['user_name']);
    $db->query("INSERT INTO users (uid, name) VALUES ('$uid', '$name')");
    $success = "Credencial agregada correctamente";
}

// Eliminar credencial
if (isset($_POST['delete_badge'])) {
    $id = intval($_POST['delete_id']);
    $db->query("DELETE FROM users WHERE id = $id");
    $success = "Credencial eliminada correctamente";
}

// Control manual de la puerta
if (isset($_POST['open_door'])) {
    file_put_contents('door_command.txt', 'OPEN');
    $success = "Comando de apertura enviado";
}
?>

<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <title>Administracion RFID</title>
    <style>
        :root {
            --primary: #4361ee;
            --secondary: #3f37c9;
            --success: #4cc9f0;
            --danger: #f72585;
            --light: #f8f9fa;
            --dark: #212529;
            --gray: #6c757d;
        }

        body {
            margin: 0;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background-color: #f5f7fa;
        }

        .container {
            margin-left: 240px;
            padding: 30px;
        }

        nav.sidebar {
            position: fixed;
            top: 0;
            left: 0;
            width: 220px;
            height: 100vh;
            background-color: var(--dark);
            color: white;
            padding: 20px;
        }

        nav.sidebar h2 {
            font-size: 20px;
            margin-bottom: 30px;
        }

        nav.sidebar ul {
            list-style: none;
            padding: 0;
        }

        nav.sidebar li {
            margin-bottom: 20px;
        }

        nav.sidebar a {
            color: white;
            text-decoration: none;
        }

        header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 30px;
        }

        h1 {
            color: var(--primary);
        }

        h2 {
            color: var(--secondary);
            margin-top: 30px;
        }

        h3 {
            margin-top: 20px;
            color: var(--gray);
        }

        .btn {
            background: var(--primary);
            color: white;
            padding: 10px 20px;
            border: none;
            border-radius: 6px;
            cursor: pointer;
            text-decoration: none;
        }

        .btn:hover {
            background: var(--secondary);
        }

        .btn-danger {
            background: var(--danger);
        }

        .btn-danger:hover {
            background: #d1144a;
        }

        .btn-success {
            background: var(--success);
        }

        .btn-success:hover {
            background: #2ab4d6;
        }

        table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 20px;
        }

        th, td {
            padding: 12px;
            border-bottom: 1px solid #ddd;
        }

        th {
            background-color: var(--primary);
            color: white;
        }

        tr:nth-child(even) {
            background-color: #f9f9f9;
        }

        .section {
            margin-bottom: 50px;
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.05);
        }

        .alert {
            padding: 15px;
            margin-bottom: 20px;
            border-radius: 6px;
        }

        .alert-success {
            background-color: #d4edda;
            color: #155724;
        }

        .alert-danger {
            background-color: #f8d7da;
            color: #721c24;
        }

        input[type="text"], input[type="password"] {
            padding: 10px;
            width: 300px;
            margin-bottom: 10px;
            border-radius: 6px;
            border: 1px solid #ccc;
        }

        .login-container {
            max-width: 400px;
            margin: 100px auto;
            background: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 0 20px rgba(0,0,0,0.1);
        }
    </style>
</head>
<body>

<?php if (!isset($_SESSION['loggedin'])): ?>
    <div class="login-container">
        <h2>Inicio de sesion del administrador</h2>
        <?php if (isset($error)): ?>
            <div class="alert alert-danger"><?= $error ?></div>
        <?php endif; ?>
        <form method="POST">
            <input type="text" name="username" placeholder="Usuario" required><br>
            <input type="password" name="password" placeholder="Contrasena" required><br>
            <button type="submit" name="login" class="btn">Iniciar sesion</button>
        </form>
    </div>
<?php else: ?>

    <!-- SIDEBAR -->
    <nav class="sidebar">
        <h2>Menu Admin</h2>
        <ul>
            <li><a href="#badges">Credenciales</a></li>
            <li><a href="#logs">Historial</a></li>
            <li><a href="#control">Control</a></li>
            <li><a href="?logout" class="btn btn-danger btn-sm">Cerrar sesion</a></li>
        </ul>
    </nav>

    <!-- CONTENU PRINCIPAL -->
    <div class="container">
        <header>
            <h1>Control de acceso RFID</h1>
        </header>

        <?php if (isset($success)): ?>
            <div class="alert alert-success"><?= $success ?></div>
        <?php endif; ?>

        <div class="section" id="badges">
            <h2>Gestion de credenciales</h2>
            <form method="POST">
                <input type="text" name="new_uid" placeholder="UID de la credencial" required>
                <input type="text" name="user_name" placeholder="Nombre del usuario" required>
                <button type="submit" name="add_badge" class="btn btn-success">Agregar credencial</button>
            </form>

            <h3>Credenciales registradas</h3>
            <table>
                <thead>
                    <tr><th>ID</th><th>UID</th><th>Nombre</th><th>Fecha</th><th>Acciones</th></tr>
                </thead>
                <tbody>
                    <?php
                    $badges = $db->query("SELECT * FROM users ORDER BY created_at DESC");
                    while ($row = $badges->fetch_assoc()):
                    ?>
                    <tr>
                        <td><?= $row['id'] ?></td>
                        <td><?= $row['uid'] ?></td>
                        <td><?= $row['name'] ?></td>
                        <td><?= format_utc_timestamp($row['created_at'], $utc_timezone, $app_timezone) ?></td>
                        <td>
                            <form method="POST" onsubmit="return confirm('Eliminar esta credencial?');">
                                <input type="hidden" name="delete_id" value="<?= $row['id'] ?>">
                                <button type="submit" name="delete_badge" class="btn btn-danger btn-sm">Eliminar</button>
                            </form>
                        </td>
                    </tr>
                    <?php endwhile; ?>
                </tbody>
            </table>
        </div>

        <div class="section" id="logs">
            <h2>Historial de accesos</h2>
            <a href="?export" class="btn">Exportar CSV</a>
            <table>
                <thead>
                    <tr><th>ID</th><th>UID</th><th>Estado</th><th>Fecha/Hora</th></tr>
                </thead>
                <tbody>
                    <?php
                    $logs = $db->query("SELECT * FROM logs ORDER BY timestamp DESC LIMIT 100");
                    while ($row = $logs->fetch_assoc()):
                    ?>
                    <tr>
                        <td><?= $row['id'] ?></td>
                        <td><?= $row['uid'] ?></td>
                        <td style="color: <?= $row['access_status'] === 'GRANTED' ? 'green' : 'red' ?>;">
                            <?= $row['access_status'] === 'GRANTED' ? 'Acceso permitido' : 'Acceso denegado' ?>
                        </td>
                        <td><?= format_utc_timestamp($row['timestamp'], $utc_timezone, $app_timezone) ?></td>
                    </tr>
                    <?php endwhile; ?>
                </tbody>
            </table>
        </div>

        <div class="section" id="control">
            <h2>Control manual</h2>
            <form method="POST" onsubmit="return confirm('Abrir la puerta ahora?');">
                <button type="submit" name="open_door" class="btn">Abrir puerta</button>
            </form>
        </div>
    </div>
<?php endif; ?>
</body>
</html>

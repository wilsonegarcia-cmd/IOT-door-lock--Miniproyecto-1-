<?php
error_reporting(E_ALL);
ini_set('display_errors', 1);
header('Content-Type: text/plain');

$app_timezone_name = getenv('APP_TIMEZONE') ?: 'America/Guayaquil';
date_default_timezone_set($app_timezone_name);

// Conexion a MySQL (usa variables de entorno en Docker y valores por defecto en XAMPP)
$db_host = getenv('DB_HOST') ?: 'localhost';
$db_user = getenv('DB_USER') ?: 'root';
$db_pass = getenv('DB_PASS') ?: '';
$db_name = getenv('DB_NAME') ?: 'rfid_access';
$db = new mysqli($db_host, $db_user, $db_pass, $db_name);

// Verificar si fallo la conexion
if ($db->connect_error) {
    die("Error de conexion MySQL: " . $db->connect_error);
}

$db->query("SET time_zone = '+00:00'");

// Obtener el UID enviado por el ESP32 (via GET)
$uid = $_GET['uid'] ?? '';

if (empty($uid)) {
    die("DENIED"); // UID vacio = acceso denegado
}

// Verificar si el UID existe en la tabla 'users'
$query = $db->prepare("SELECT id FROM users WHERE uid = ?");
$query->bind_param('s', $uid);
$query->execute();
$result = $query->get_result();

if ($result->num_rows > 0) {
    // UID encontrado: acceso permitido
    echo "GRANTED";

    // Registrar log de acceso permitido
    $logQuery = $db->prepare("INSERT INTO logs (uid, access_status) VALUES (?, 'GRANTED')");
    $logQuery->bind_param('s', $uid);
    $logQuery->execute();
} else {
    // UID desconocido: acceso denegado
    echo "DENIED";

    // Registrar log de acceso denegado
    $logQuery = $db->prepare("INSERT INTO logs (uid, access_status) VALUES (?, 'DENIED')");
    $logQuery->bind_param('s', $uid);
    $logQuery->execute();
}

$db->close();
?>

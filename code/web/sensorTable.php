<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="3">
    <title>Beacon Log Monitor</title>

    <style>
        body {
            font-family: Arial, sans-serif;
        }
        table {
            border-collapse: collapse;
            width: 90%;
            margin: 20px auto;
            background: white;
        }
        th, td {
            border: 1px solid #444;
            padding: 8px;
            text-align: center;
            font-size: 14px;
        }
        th {
            background: #eee;
        }
        tr:nth-child(even) {
            background: #f2f2f2;
        }
    </style>
</head>

<body>

<h2 style="text-align:center;">📡 Beacon Log (Latest 100)</h2>

<?php
$conn = mysqli_connect("localhost", "iot", "pwiot", "iotdb");
if (!$conn) {
    die("DB 연결 실패: " . mysqli_connect_error());
}

$sql = "
    SELECT ts, reporter, uuid, rssi, distance_m
    FROM beacon_log
    ORDER BY ts DESC
    LIMIT 100
";

$result = mysqli_query($conn, $sql);
if (!$result) {
    die("쿼리 실패: " . mysqli_error($conn));
}
?>

<table>
    <tr>
        <th>No</th>
        <th>Time</th>
        <th>ITEM</th>
        <th>RSSI</th>
        <th>Distance (m)</th>
        <th>Status</th>
    </tr>

<?php
$no = 1;
while ($row = mysqli_fetch_assoc($result)) {

    $dist = (float)$row['distance_m'];

    
    if ($dist >= 10.0) {
        $status = "VERY FAR";
        $color  = "#e74c3c";
    } elseif ($dist >= 5.0) {
        $status = "FAR";
        $color  = "#f39c12";
    } elseif ($dist >= 1.0) {
        $status = "NEAR";
        $color  = "#27ae60";
    } else {
        $status = "VERY NEAR";
        $color  = "#2ecc71";
    }

    echo "<tr>";
    echo "<td>{$no}</td>";
    echo "<td>{$row['ts']}</td>";
    echo "<td><b>WALLET TAG</b></td>";
    echo "<td>{$row['rssi']}</td>";
    echo "<td>" . number_format($dist, 3) . "</td>";
    echo "<td style='color:$color;font-weight:bold'>{$status}</td>";
    echo "</tr>";

    $no++;
}

if ($no === 1) {
    echo "<tr><td colspan='7'>데이터 없음</td></tr>";
}

mysqli_close($conn);
?>

</table>

</body>
</html>

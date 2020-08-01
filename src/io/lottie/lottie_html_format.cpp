#include "lottie_html_format.hpp"
#include "lottie_format.hpp"



io::Autoreg<io::lottie::LottieHtmlFormat> io::lottie::LottieHtmlFormat::autoreg;


bool io::lottie::LottieHtmlFormat::on_save(QIODevice& file, const QString&,
                                           model::Document* document, const QVariantMap&)
{
    file.write(QString(
R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8" />
    <style>
        html, body { width: 100%; height: 100%; margin: 0; }
        body { display: flex; }
        #bodymovin { width: %1px; height: %2px; margin: auto;
            background-color: white;
            background-size: 64px 64px;
            background-image:
                linear-gradient(to right, rgba(0, 0, 0, .3) 50%, transparent 50%),
                linear-gradient(to bottom, rgba(0, 0, 0, .3) 50%, transparent 50%),
                linear-gradient(to bottom, white 50%, transparent 50%),
                linear-gradient(to right, transparent 50%, rgba(0, 0, 0, .5) 50%);
        }
    </style>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/bodymovin/5.5.3/lottie.js"></script>
</head>
<body>
<div id="bodymovin"></div>

<script>
    var animData = {
        container: document.getElementById('bodymovin'),
        renderer: 'svg',
        loop: true,
        autoplay: true,
        animationData:
)")
    .arg(document->animation()->width.get())
    .arg(document->animation()->height.get())
    .toUtf8()
    );

    file.write(LottieFormat::to_json(document).toJson());

    file.write(R"(
    };
    var anim = bodymovin.loadAnimation(animData);
</script>
</body></html>
)");

    return true;
}
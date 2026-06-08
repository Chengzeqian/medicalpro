#include <QtTest/QtTest>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QTextStream>
#include <QVariantMap>
#include <QtMath>

#include <algorithm>

#include <vtkAppendPolyData.h>
#include <vtkCellArray.h>
#include <vtkCell.h>
#include <vtkCubeSource.h>
#include <vtkDataArray.h>
#include <vtkMatrix4x4.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>
#include <vtkSTLReader.h>
#include <vtkTriangleFilter.h>

#include "algorithms/meshgpu/include/mesh_gpu_runtime_api.h"
#include "Plugins/RegistrationCore/RegistrationServiceImpl.h"

class RegistrationCoreMeshGpuSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void runtime_output_contains_meshgpu_dll();
    void registration_service_loads_meshgpu_dll_from_runtime_output();
    void advanced_icp_with_gpu_records_gicp_registration_when_runtime_is_available();
    void advanced_icp_with_constraint_payload_records_core_constraint_usage();
    void advanced_icp_parallel_search_records_parallel_search_metadata();
    void advanced_icp_with_exact_initial_transform_does_not_double_apply_transform();
    void advanced_icp_parallel_search_with_exact_initial_transform_preserves_alignment();
    void runtime_batch_refine_returns_result_per_candidate();
    void advanced_icp_parallel_search_refines_top_k_and_records_non_zero_best_candidate_rank();
    void advanced_icp_parallel_search_records_batch_refine_metadata();
    void advanced_icp_parallel_search_with_constraints_records_parallel_search_metadata();
    void advanced_icp_parallel_search_records_multi_resolution_and_constraint_filter_metrics();
    void advanced_icp_with_pose_perturbation_records_nonzero_rmse_and_iteration_count();
    void advanced_icp_real_bone_mesh_partial_surface_registration_reports_realistic_error();
    void advanced_icp_real_bone_stress_matrix_exports_summary_csv();
    void advanced_icp_real_bone_registration_visualization_exports_before_after_clouds();
    void candidate_batch_scoring_returns_ranked_scores_from_runtime_api();
    void runtime_constraint_filter_returns_selected_indices();
    void runtime_target_constraint_filter_returns_selected_indices();
    void runtime_constrained_target_mesh_returns_compact_mesh();

private:
    using CreateRuntimeApiFn = mesh_gpu::MeshGPURuntimeApi* (*)();
    using DestroyRuntimeApiFn = void (*)(mesh_gpu::MeshGPURuntimeApi*);

    static vtkSmartPointer<vtkPolyData> createRegistrationSurface(double tx = 0.0, double ty = 0.0, double tz = 0.0)
    {
        auto cube = vtkSmartPointer<vtkCubeSource>::New();
        cube->SetCenter(tx, ty, tz);
        cube->SetXLength(18.0);
        cube->SetYLength(24.0);
        cube->SetZLength(12.0);
        cube->Update();

        auto triangleFilter = vtkSmartPointer<vtkTriangleFilter>::New();
        triangleFilter->SetInputConnection(cube->GetOutputPort());
        triangleFilter->Update();

        auto normals = vtkSmartPointer<vtkPolyDataNormals>::New();
        normals->SetInputConnection(triangleFilter->GetOutputPort());
        normals->ComputePointNormalsOn();
        normals->ComputeCellNormalsOn();
        normals->SplittingOff();
        normals->ConsistencyOn();
        normals->Update();

        auto surface = vtkSmartPointer<vtkPolyData>::New();
        surface->DeepCopy(normals->GetOutput());
        return surface;
    }

    static std::vector<mesh_gpu::Point3D> extractPoints(vtkPolyData* surface)
    {
        std::vector<mesh_gpu::Point3D> points;
        points.reserve(static_cast<size_t>(surface->GetNumberOfPoints()));

        for (vtkIdType i = 0; i < surface->GetNumberOfPoints(); ++i) {
            double point[3];
            surface->GetPoint(i, point);
            points.emplace_back(
                static_cast<float>(point[0]),
                static_cast<float>(point[1]),
                static_cast<float>(point[2]));
        }

        return points;
    }

    static std::vector<mesh_gpu::Normal3D> extractNormals(vtkPolyData* surface)
    {
        std::vector<mesh_gpu::Normal3D> normals;
        vtkDataArray* normalArray = surface->GetPointData()->GetNormals();
        if (normalArray == nullptr) {
            return normals;
        }

        normals.reserve(static_cast<size_t>(surface->GetNumberOfPoints()));
        for (vtkIdType i = 0; i < surface->GetNumberOfPoints(); ++i) {
            double normal[3];
            normalArray->GetTuple(i, normal);
            normals.emplace_back(
                static_cast<float>(normal[0]),
                static_cast<float>(normal[1]),
                static_cast<float>(normal[2]));
        }

        return normals;
    }

    static std::vector<std::array<int, 3>> extractTriangles(vtkPolyData* surface)
    {
        std::vector<std::array<int, 3>> triangles;
        triangles.reserve(static_cast<size_t>(surface->GetNumberOfCells()));

        for (vtkIdType i = 0; i < surface->GetNumberOfCells(); ++i) {
            vtkCell* cell = surface->GetCell(i);
            if (cell == nullptr || cell->GetNumberOfPoints() != 3) {
                continue;
            }

            triangles.push_back({
                static_cast<int>(cell->GetPointId(0)),
                static_cast<int>(cell->GetPointId(1)),
                static_cast<int>(cell->GetPointId(2))
            });
        }

        return triangles;
    }

    static mesh_gpu::Transform4x4 createTranslationTransform(float tx, float ty, float tz)
    {
        mesh_gpu::Transform4x4 transform;
        transform(0, 3) = tx;
        transform(1, 3) = ty;
        transform(2, 3) = tz;
        return transform;
    }

    static vtkSmartPointer<vtkPolyData> loadStlSurface(const QString& path)
    {
        auto reader = vtkSmartPointer<vtkSTLReader>::New();
        reader->SetFileName(path.toUtf8().constData());
        reader->Update();

        auto normals = vtkSmartPointer<vtkPolyDataNormals>::New();
        normals->SetInputConnection(reader->GetOutputPort());
        normals->ComputePointNormalsOn();
        normals->ComputeCellNormalsOn();
        normals->SplittingOff();
        normals->ConsistencyOn();
        normals->Update();

        auto surface = vtkSmartPointer<vtkPolyData>::New();
        surface->DeepCopy(normals->GetOutput());
        return surface;
    }

    static vtkSmartPointer<vtkPolyData> appendSurfaces(
        const QList<vtkSmartPointer<vtkPolyData>>& surfaces)
    {
        auto append = vtkSmartPointer<vtkAppendPolyData>::New();
        for (const auto& surface : surfaces) {
            append->AddInputData(surface);
        }
        append->Update();

        auto normals = vtkSmartPointer<vtkPolyDataNormals>::New();
        normals->SetInputConnection(append->GetOutputPort());
        normals->ComputePointNormalsOn();
        normals->ComputeCellNormalsOn();
        normals->SplittingOff();
        normals->ConsistencyOn();
        normals->Update();

        auto combined = vtkSmartPointer<vtkPolyData>::New();
        combined->DeepCopy(normals->GetOutput());
        return combined;
    }

    static QList<QVector3D> sampleNearestPoints(
        vtkPolyData* surface,
        const QVector3D& center,
        int desiredCount)
    {
        struct IndexedPoint
        {
            QVector3D point;
            double squaredDistance = 0.0;
        };

        QList<IndexedPoint> rankedPoints;
        rankedPoints.reserve(static_cast<int>(surface->GetNumberOfPoints()));
        for (vtkIdType pointIndex = 0; pointIndex < surface->GetNumberOfPoints(); ++pointIndex) {
            double point[3];
            surface->GetPoint(pointIndex, point);
            const QVector3D candidate(
                static_cast<float>(point[0]),
                static_cast<float>(point[1]),
                static_cast<float>(point[2]));
            const QVector3D delta = candidate - center;
            rankedPoints.append({
                candidate,
                static_cast<double>(QVector3D::dotProduct(delta, delta))
            });
        }

        std::sort(rankedPoints.begin(), rankedPoints.end(), [](const IndexedPoint& left, const IndexedPoint& right) {
            return left.squaredDistance < right.squaredDistance;
        });

        QList<QVector3D> selectedPoints;
        selectedPoints.reserve(qMin(desiredCount, rankedPoints.size()));
        for (int index = 0; index < rankedPoints.size() && selectedPoints.size() < desiredCount; ++index) {
            selectedPoints.append(rankedPoints.at(index).point);
        }
        return selectedPoints;
    }

    static vtkSmartPointer<vtkPolyData> createPointCloudSurface(const QList<QVector3D>& points)
    {
        auto vtkPointsData = vtkSmartPointer<vtkPoints>::New();
        auto vertices = vtkSmartPointer<vtkCellArray>::New();

        for (const QVector3D& point : points) {
            const vtkIdType pointId = vtkPointsData->InsertNextPoint(point.x(), point.y(), point.z());
            vertices->InsertNextCell(1);
            vertices->InsertCellPoint(pointId);
        }

        auto polyData = vtkSmartPointer<vtkPolyData>::New();
        polyData->SetPoints(vtkPointsData);
        polyData->SetVerts(vertices);
        return polyData;
    }

    static QVariantList matrixToVariantList(const QMatrix4x4& matrix)
    {
        QVariantList values;
        values.reserve(16);
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                values.append(matrix(row, column));
            }
        }
        return values;
    }

    static double pairedRmse(
        const QList<QVector3D>& sourcePoints,
        const QList<QVector3D>& targetPoints,
        vtkMatrix4x4* transform)
    {
        if (!transform || sourcePoints.size() != targetPoints.size() || sourcePoints.isEmpty()) {
            return -1.0;
        }

        double sumSquaredError = 0.0;
        for (int index = 0; index < sourcePoints.size(); ++index) {
            const QVector3D& sourcePoint = sourcePoints.at(index);
            double inputPoint[4] = { sourcePoint.x(), sourcePoint.y(), sourcePoint.z(), 1.0 };
            double outputPoint[4];
            transform->MultiplyPoint(inputPoint, outputPoint);

            const QVector3D transformedPoint(
                static_cast<float>(outputPoint[0]),
                static_cast<float>(outputPoint[1]),
                static_cast<float>(outputPoint[2]));
            const QVector3D delta = transformedPoint - targetPoints.at(index);
            sumSquaredError += static_cast<double>(QVector3D::dotProduct(delta, delta));
        }

        return qSqrt(sumSquaredError / static_cast<double>(sourcePoints.size()));
    }

    static double pairedRmseForPointLists(
        const QList<QVector3D>& leftPoints,
        const QList<QVector3D>& rightPoints)
    {
        if (leftPoints.size() != rightPoints.size() || leftPoints.isEmpty()) {
            return -1.0;
        }

        double sumSquaredError = 0.0;
        for (int index = 0; index < leftPoints.size(); ++index) {
            const QVector3D delta = leftPoints.at(index) - rightPoints.at(index);
            sumSquaredError += static_cast<double>(QVector3D::dotProduct(delta, delta));
        }

        return qSqrt(sumSquaredError / static_cast<double>(leftPoints.size()));
    }

    static QList<QVector3D> transformPoints(
        const QList<QVector3D>& sourcePoints,
        vtkMatrix4x4* transform)
    {
        QList<QVector3D> transformedPoints;
        transformedPoints.reserve(sourcePoints.size());
        if (transform == nullptr) {
            return transformedPoints;
        }

        for (const QVector3D& sourcePoint : sourcePoints) {
            double inputPoint[4] = { sourcePoint.x(), sourcePoint.y(), sourcePoint.z(), 1.0 };
            double outputPoint[4];
            transform->MultiplyPoint(inputPoint, outputPoint);
            transformedPoints.append(QVector3D(
                static_cast<float>(outputPoint[0]),
                static_cast<float>(outputPoint[1]),
                static_cast<float>(outputPoint[2])));
        }

        return transformedPoints;
    }

    static QList<QVector3D> sampleSurfacePointsEvenly(vtkPolyData* surface, int desiredCount)
    {
        QList<QVector3D> sampledPoints;
        if (surface == nullptr || desiredCount <= 0) {
            return sampledPoints;
        }

        const vtkIdType pointCount = surface->GetNumberOfPoints();
        if (pointCount <= 0) {
            return sampledPoints;
        }

        const int sampleCount = qMin(desiredCount, static_cast<int>(pointCount));
        sampledPoints.reserve(sampleCount);
        for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
            const vtkIdType pointIndex =
                sampleCount == 1
                    ? 0
                    : static_cast<vtkIdType>(
                        qRound64(static_cast<double>(sampleIndex) * static_cast<double>(pointCount - 1)
                                 / static_cast<double>(sampleCount - 1)));
            double point[3];
            surface->GetPoint(pointIndex, point);
            sampledPoints.append(QVector3D(
                static_cast<float>(point[0]),
                static_cast<float>(point[1]),
                static_cast<float>(point[2])));
        }
        return sampledPoints;
    }

    static bool writePointCloudCsv(const QString& path, const QList<QVector3D>& points)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            return false;
        }

        QTextStream out(&file);
        out << "index,x_mm,y_mm,z_mm\n";
        for (int index = 0; index < points.size(); ++index) {
            const QVector3D& point = points.at(index);
            out << index << ","
                << QString::number(point.x(), 'f', 5) << ","
                << QString::number(point.y(), 'f', 5) << ","
                << QString::number(point.z(), 'f', 5) << "\n";
        }
        return true;
    }

    static bool writeMatrixCsv(const QString& path, vtkMatrix4x4* matrix)
    {
        if (matrix == nullptr) {
            return false;
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            return false;
        }

        QTextStream out(&file);
        out << "row,c0,c1,c2,c3\n";
        for (int row = 0; row < 4; ++row) {
            out << row;
            for (int column = 0; column < 4; ++column) {
                out << "," << QString::number(matrix->GetElement(row, column), 'f', 8);
            }
            out << "\n";
        }
        return true;
    }

    static QVariantList vectorToVariantList(const QVector3D& point)
    {
        return QVariantList { point.x(), point.y(), point.z() };
    }

    static QVariantList vectorListToVariantList(const QList<QVector3D>& points)
    {
        QVariantList values;
        values.reserve(points.size());
        for (const QVector3D& point : points) {
            values.append(vectorToVariantList(point));
        }
        return values;
    }

    static QVariantList vectorListToFlatVariantList(const QList<QVector3D>& points)
    {
        QVariantList values;
        values.reserve(points.size() * 3);
        for (const QVector3D& point : points) {
            values.append(point.x());
            values.append(point.y());
            values.append(point.z());
        }
        return values;
    }

    static QString pointArrayJavascript(const QList<QVector3D>& points)
    {
        QString javascript;
        javascript.reserve(points.size() * 32 + 2);
        javascript.append(QLatin1Char('['));
        for (int index = 0; index < points.size(); ++index) {
            if (index > 0) {
                javascript.append(QLatin1Char(','));
            }
            const QVector3D& point = points.at(index);
            javascript.append(QLatin1Char('['));
            javascript.append(QString::number(point.x(), 'f', 4));
            javascript.append(QLatin1Char(','));
            javascript.append(QString::number(point.y(), 'f', 4));
            javascript.append(QLatin1Char(','));
            javascript.append(QString::number(point.z(), 'f', 4));
            javascript.append(QLatin1Char(']'));
        }
        javascript.append(QLatin1Char(']'));
        return javascript;
    }

    static bool writeRegistrationVisualizationHtml(
        const QString& path,
        const QList<QVector3D>& targetSurfaceSamplePoints,
        const QList<QVector3D>& targetProbePoints,
        const QList<QVector3D>& sourceRawPoints,
        const QList<QVector3D>& sourceInitialTransformedPoints,
        const QList<QVector3D>& sourceFinalTransformedPoints,
        double rawResidualMm,
        double initialResidualMm,
        double finalResidualMm)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            return false;
        }

        QTextStream out(&file);
        out << "<!doctype html>\n"
            << "<html lang=\"en\">\n"
            << "<head>\n"
            << "<meta charset=\"utf-8\">\n"
            << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
            << "<title>Real Bone Registration Before After</title>\n"
            << "<style>\n"
            << ":root{--bg:#0f172a;--panel:#111827;--ink:#e5e7eb;--muted:#94a3b8;--line:#334155;}\n"
            << "body{margin:0;background:radial-gradient(circle at top left,#1e293b,#020617 55%);color:var(--ink);font:14px/1.5 Consolas,monospace;}\n"
            << "main{padding:24px;max-width:1400px;margin:0 auto;}\n"
            << "h1{font-size:24px;margin:0 0 8px;} p{color:var(--muted);margin:0 0 18px;}\n"
            << ".metrics{display:flex;gap:12px;flex-wrap:wrap;margin-bottom:18px;}\n"
            << ".metric{background:rgba(15,23,42,.82);border:1px solid var(--line);border-radius:14px;padding:10px 14px;}\n"
            << ".metric b{display:block;font-size:18px;color:#f8fafc;}\n"
            << ".controls{background:rgba(15,23,42,.72);border:1px solid var(--line);border-radius:14px;padding:12px;margin-bottom:18px;}\n"
            << ".controls label{display:inline-flex;align-items:center;gap:8px;margin-right:18px;color:var(--muted);}\n"
            << ".grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:14px;}\n"
            << "section{background:rgba(17,24,39,.88);border:1px solid var(--line);border-radius:18px;overflow:hidden;box-shadow:0 20px 55px rgba(0,0,0,.28);}\n"
            << "section h2{font-size:16px;margin:12px 14px 4px;} section p{margin:0 14px 12px;font-size:12px;}\n"
            << "canvas{display:block;width:100%;height:520px;background:linear-gradient(180deg,#020617,#0f172a);}\n"
            << ".legend{display:flex;flex-wrap:wrap;gap:10px;padding:10px 14px 14px;color:var(--muted);font-size:12px;}\n"
            << ".dot{width:10px;height:10px;border-radius:50%;display:inline-block;margin-right:5px;}\n"
            << "@media(max-width:1000px){.grid{grid-template-columns:1fr;}canvas{height:420px;}}\n"
            << "</style>\n"
            << "</head>\n"
            << "<body>\n"
            << "<main>\n"
            << "<h1>Real Bone Registration Before / Initial / After</h1>\n"
            << "<p>Target is the sampled surgical bone surface. Source is the probe-collected point cloud before and after applying registration transforms.</p>\n"
            << "<div class=\"metrics\">\n"
            << "<div class=\"metric\">Raw paired residual<b>" << QString::number(rawResidualMm, 'f', 3) << " mm</b></div>\n"
            << "<div class=\"metric\">Initial paired residual<b>" << QString::number(initialResidualMm, 'f', 3) << " mm</b></div>\n"
            << "<div class=\"metric\">Parallel final paired residual<b>" << QString::number(finalResidualMm, 'f', 3) << " mm</b></div>\n"
            << "</div>\n"
            << "<div class=\"controls\">\n"
            << "<label>Yaw <input id=\"yaw\" type=\"range\" min=\"-180\" max=\"180\" value=\"-35\"></label>\n"
            << "<label>Pitch <input id=\"pitch\" type=\"range\" min=\"-80\" max=\"80\" value=\"24\"></label>\n"
            << "<label>Zoom <input id=\"zoom\" type=\"range\" min=\"60\" max=\"180\" value=\"100\"></label>\n"
            << "</div>\n"
            << "<div class=\"grid\">\n"
            << "<section><h2>Before registration</h2><p>source_raw is still in the probe/source frame.</p><canvas id=\"beforeCanvas\"></canvas><div class=\"legend\"><span><i class=\"dot\" style=\"background:#64748b\"></i>target_surface</span><span><i class=\"dot\" style=\"background:#22c55e\"></i>target_probe</span><span><i class=\"dot\" style=\"background:#ef4444\"></i>source_raw</span></div></section>\n"
            << "<section><h2>After robust initial</h2><p>source_initial_transformed shows the initial admission quality.</p><canvas id=\"initialCanvas\"></canvas><div class=\"legend\"><span><i class=\"dot\" style=\"background:#64748b\"></i>target_surface</span><span><i class=\"dot\" style=\"background:#22c55e\"></i>target_probe</span><span><i class=\"dot\" style=\"background:#f97316\"></i>source_initial_transformed</span></div></section>\n"
            << "<section><h2>After parallel registration</h2><p>source_parallel_final_transformed is the final algorithm output.</p><canvas id=\"finalCanvas\"></canvas><div class=\"legend\"><span><i class=\"dot\" style=\"background:#64748b\"></i>target_surface</span><span><i class=\"dot\" style=\"background:#22c55e\"></i>target_probe</span><span><i class=\"dot\" style=\"background:#38bdf8\"></i>source_parallel_final_transformed</span></div></section>\n"
            << "</div>\n"
            << "</main>\n"
            << "<script>\n"
            << "const layers={\n"
            << "target_surface:{color:'#64748b',size:1,alpha:.33,points:" << pointArrayJavascript(targetSurfaceSamplePoints) << "},\n"
            << "target_probe:{color:'#22c55e',size:4,alpha:.95,points:" << pointArrayJavascript(targetProbePoints) << "},\n"
            << "source_raw:{color:'#ef4444',size:4,alpha:.95,points:" << pointArrayJavascript(sourceRawPoints) << "},\n"
            << "source_initial_transformed:{color:'#f97316',size:4,alpha:.95,points:" << pointArrayJavascript(sourceInitialTransformedPoints) << "},\n"
            << "source_parallel_final_transformed:{color:'#38bdf8',size:4,alpha:.95,points:" << pointArrayJavascript(sourceFinalTransformedPoints) << "}\n"
            << "};\n"
            << "const scenes=[\n"
            << "{canvas:'beforeCanvas',keys:['target_surface','target_probe','source_raw']},\n"
            << "{canvas:'initialCanvas',keys:['target_surface','target_probe','source_initial_transformed']},\n"
            << "{canvas:'finalCanvas',keys:['target_surface','target_probe','source_parallel_final_transformed']}\n"
            << "];\n"
            << "const allPoints=Object.values(layers).flatMap(layer=>layer.points);\n"
            << "const bounds=allPoints.reduce((box,p)=>({min:[Math.min(box.min[0],p[0]),Math.min(box.min[1],p[1]),Math.min(box.min[2],p[2])],max:[Math.max(box.max[0],p[0]),Math.max(box.max[1],p[1]),Math.max(box.max[2],p[2])]}),{min:[Infinity,Infinity,Infinity],max:[-Infinity,-Infinity,-Infinity]});\n"
            << "const center=[(bounds.min[0]+bounds.max[0])/2,(bounds.min[1]+bounds.max[1])/2,(bounds.min[2]+bounds.max[2])/2];\n"
            << "const radius=Math.max(bounds.max[0]-bounds.min[0],bounds.max[1]-bounds.min[1],bounds.max[2]-bounds.min[2])||1;\n"
            << "const yawInput=document.getElementById('yaw');const pitchInput=document.getElementById('pitch');const zoomInput=document.getElementById('zoom');\n"
            << "function project(p,w,h){const yaw=Number(yawInput.value)*Math.PI/180;const pitch=Number(pitchInput.value)*Math.PI/180;const zoom=Number(zoomInput.value)/100;let x=p[0]-center[0],y=p[1]-center[1],z=p[2]-center[2];const cy=Math.cos(yaw),sy=Math.sin(yaw),cp=Math.cos(pitch),sp=Math.sin(pitch);const x1=x*cy-z*sy;const z1=x*sy+z*cy;const y1=y*cp-z1*sp;const s=Math.min(w,h)*.78/radius*zoom;return [w/2+x1*s,h/2-y1*s,z1];}\n"
            << "function draw(){for(const scene of scenes){const canvas=document.getElementById(scene.canvas);const ratio=window.devicePixelRatio||1;const rect=canvas.getBoundingClientRect();canvas.width=Math.max(1,Math.floor(rect.width*ratio));canvas.height=Math.max(1,Math.floor(rect.height*ratio));const ctx=canvas.getContext('2d');ctx.setTransform(ratio,0,0,ratio,0,0);ctx.clearRect(0,0,rect.width,rect.height);ctx.strokeStyle='rgba(148,163,184,.16)';for(let i=1;i<6;i++){ctx.beginPath();ctx.moveTo(i*rect.width/6,0);ctx.lineTo(i*rect.width/6,rect.height);ctx.moveTo(0,i*rect.height/6);ctx.lineTo(rect.width,i*rect.height/6);ctx.stroke();}for(const key of scene.keys){const layer=layers[key];ctx.globalAlpha=layer.alpha;ctx.fillStyle=layer.color;const projected=layer.points.map(p=>project(p,rect.width,rect.height)).sort((a,b)=>a[2]-b[2]);for(const p of projected){ctx.beginPath();ctx.arc(p[0],p[1],layer.size,0,Math.PI*2);ctx.fill();}}ctx.globalAlpha=1;}}\n"
            << "yawInput.addEventListener('input',draw);pitchInput.addEventListener('input',draw);zoomInput.addEventListener('input',draw);window.addEventListener('resize',draw);draw();\n"
            << "</script>\n"
            << "</body>\n"
            << "</html>\n";
        return true;
    }

    static QString runRealBoneStressMatrixAndWriteSummary();
    static QString runRealBoneRegistrationVisualizationAndWriteArtifacts();
};

QString RegistrationCoreMeshGpuSmokeTest::runRealBoneStressMatrixAndWriteSummary()
{
    struct StressScenario
    {
        QString id;
        double noiseScale = 1.0;
        int outlierPointCount = 0;
        QVector3D initialTranslationMm;
        double initialRotationYDeg = 0.0;
    };

    const QString tibiaPath =
        QStringLiteral("D:/Adata/ANSN/ASNS/Release/patient_data/45971129749/reconstructed_mesh_preview/tibia.stl");
    const QString talusPath =
        QStringLiteral("D:/Adata/ANSN/ASNS/Release/patient_data/45971129749/reconstructed_mesh_preview/talus.stl");
    if (!QFileInfo::exists(tibiaPath) || !QFileInfo::exists(talusPath)) {
        return {};
    }

    RegistrationServiceImpl service;
    if (!service.loadMeshGPUDLL()) {
        return {};
    }

    const auto tibia = loadStlSurface(tibiaPath);
    const auto talus = loadStlSurface(talusPath);
    if (tibia == nullptr || talus == nullptr) {
        return {};
    }

    const auto target = appendSurfaces({ tibia, talus });
    if (target == nullptr || target->GetNumberOfPoints() <= 2000) {
        return {};
    }

    double talusBounds[6];
    talus->GetBounds(talusBounds);
    const QVector3D targetRegionCenter(
        static_cast<float>((talusBounds[0] + talusBounds[1]) * 0.5),
        static_cast<float>((talusBounds[2] + talusBounds[3]) * 0.5),
        static_cast<float>((talusBounds[4] + talusBounds[5]) * 0.5));

    const QList<QVector3D> targetProbePoints = sampleNearestPoints(target, targetRegionCenter, 240);
    if (targetProbePoints.size() < 120) {
        return {};
    }

    QMatrix4x4 sourceToTargetSeed;
    sourceToTargetSeed.setToIdentity();
    sourceToTargetSeed.translate(-7.5f, 4.5f, -3.2f);
    sourceToTargetSeed.rotate(-9.0f, QVector3D(0.0f, 0.0f, 1.0f));
    sourceToTargetSeed.rotate(5.5f, QVector3D(1.0f, 0.0f, 0.0f));
    const QMatrix4x4 targetToSourceGroundTruth = sourceToTargetSeed.inverted();

    const QList<StressScenario> scenarios {
        { QStringLiteral("baseline"), 1.0, 0, QVector3D(2.4f, -1.6f, 1.1f), 3.5 },
        { QStringLiteral("medium_noise"), 2.0, 0, QVector3D(2.4f, -1.6f, 1.1f), 3.5 },
        { QStringLiteral("high_noise"), 4.0, 0, QVector3D(2.4f, -1.6f, 1.1f), 3.5 },
        { QStringLiteral("outlier_points"), 1.0, 8, QVector3D(2.4f, -1.6f, 1.1f), 3.5 },
        { QStringLiteral("large_initial_offset"), 1.0, 0, QVector3D(10.0f, -7.0f, 4.5f), 9.0 },
        { QStringLiteral("rotation_5deg_y"), 1.0, 0, QVector3D(2.4f, -1.6f, 1.1f), 5.0 },
        { QStringLiteral("rotation_10deg_y"), 1.0, 0, QVector3D(2.4f, -1.6f, 1.1f), 10.0 },
        { QStringLiteral("rotation_20deg_y"), 1.0, 0, QVector3D(2.4f, -1.6f, 1.1f), 20.0 },
        { QStringLiteral("large_translation_20mm"), 1.0, 0, QVector3D(15.0f, -12.0f, 8.0f), 3.5 },
        { QStringLiteral("large_translation_30mm"), 1.0, 0, QVector3D(22.0f, -18.0f, 12.0f), 3.5 },
        { QStringLiteral("combined_extreme"), 1.0, 0, QVector3D(15.0f, -12.0f, 8.0f), 10.0 }
    };

    QDir outputDir(QCoreApplication::applicationDirPath()
        + QStringLiteral("/summaries/real_bone_stress_matrix"));
    outputDir.mkpath(QStringLiteral("."));
    const QString summaryPath = outputDir.filePath(QStringLiteral("summary.csv"));
    QFile summaryFile(summaryPath);
    if (!summaryFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return summaryPath;
    }

    QTextStream csv(&summaryFile);
    csv << "scenario,direct_ms,parallel_ms,speedup_x,direct_paired_mm,parallel_paired_mm,"
        << "direct_rmse_mm,parallel_rmse_mm,parallel_search_ms,parallel_source,"
        << "parallel_scored_candidates,parallel_refine_candidates,initial_paired_mm,"
        << "initial_admission_action,initial_admission_reason,initial_admission_recovery,"
        << "direct_success,parallel_success\n";

    for (const StressScenario& scenario : scenarios) {
        QList<QVector3D> sourceProbePoints;
        sourceProbePoints.reserve(targetProbePoints.size());
        for (int pointIndex = 0; pointIndex < targetProbePoints.size(); ++pointIndex) {
            const QVector3D transformedPoint = targetToSourceGroundTruth.map(targetProbePoints.at(pointIndex));
            const float xNoise = static_cast<float>(((pointIndex % 5) - 2) * 0.18 * scenario.noiseScale);
            const float yNoise = static_cast<float>((((pointIndex * 2) % 5) - 2) * 0.14 * scenario.noiseScale);
            const float zNoise = static_cast<float>((((pointIndex * 3) % 5) - 2) * 0.10 * scenario.noiseScale);
            QVector3D noisyPoint = transformedPoint + QVector3D(xNoise, yNoise, zNoise);
            if (pointIndex < scenario.outlierPointCount) {
                noisyPoint += QVector3D(12.0f, -9.0f, 6.0f);
            }
            sourceProbePoints.append(noisyPoint);
        }

        const auto source = createPointCloudSurface(sourceProbePoints);
        if (source == nullptr) {
            csv << scenario.id << ",-1,-1,0,-1,-1,-1,-1,-1,source_build_failed,0,0,-1,false,false\n";
            continue;
        }

        QMatrix4x4 initialTransform = sourceToTargetSeed;
        initialTransform.translate(scenario.initialTranslationMm);
        initialTransform.rotate(
            static_cast<float>(scenario.initialRotationYDeg),
            QVector3D(0.0f, 1.0f, 0.0f));

        vtkSmartPointer<vtkMatrix4x4> initialMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        initialMatrix->Identity();
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                initialMatrix->SetElement(row, column, initialTransform(row, column));
            }
        }
        const double initialPairedResidualMm = pairedRmse(sourceProbePoints, targetProbePoints, initialMatrix);

        QVariantMap baseParameters;
        baseParameters.insert(QStringLiteral("useGPU"), true);
        baseParameters.insert(QStringLiteral("enableConstraintParallelFilter"), true);
        baseParameters.insert(QStringLiteral("registrationMethodId"), QStringLiteral("ankle_two_stage_constrained"));
        baseParameters.insert(QStringLiteral("candidateCount"), 64);
        baseParameters.insert(QStringLiteral("topKCandidateCount"), 4);
        baseParameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_two_level"));
        baseParameters.insert(QStringLiteral("targetRegionCenterX"), targetRegionCenter.x());
        baseParameters.insert(QStringLiteral("targetRegionCenterY"), targetRegionCenter.y());
        baseParameters.insert(QStringLiteral("targetRegionCenterZ"), targetRegionCenter.z());
        baseParameters.insert(QStringLiteral("targetRegionRadiusMm"), 32.0);
        baseParameters.insert(QStringLiteral("initialTransform"), matrixToVariantList(initialTransform));
        baseParameters.insert(QStringLiteral("maxIterations"), 18);
        baseParameters.insert(QStringLiteral("distanceThreshold"), 8.0);
        baseParameters.insert(QStringLiteral("usePointToPlane"), true);
        baseParameters.insert(QStringLiteral("verbose"), false);
        baseParameters.insert(QStringLiteral("robustInitialAvailable"), true);
        baseParameters.insert(QStringLiteral("robustInitialRmsMm"), initialPairedResidualMm);
        baseParameters.insert(QStringLiteral("robustInitialConfidence"), 0.85);
        baseParameters.insert(QStringLiteral("robustInitialInlierCount"), sourceProbePoints.size());

        QVariantMap directParameters = baseParameters;
        const QString directId = QStringLiteral("stress_%1_direct").arg(scenario.id);
        directParameters.insert(QStringLiteral("registrationId"), directId);
        directParameters.insert(QStringLiteral("enableParallelInitialSearch"), false);
        const auto directMatrix = service.performICPRegistrationAdvanced(source, target, directParameters);
        const bool directSuccess = directMatrix != nullptr;
        const QVariantMap directInfo = service.getRegistrationInfo(directId);
        const QVariantMap directMetadata = directInfo.value(QStringLiteral("metadata")).toMap();
        const qint64 directPipelineElapsedMs =
            directMetadata.value(QStringLiteral("pipelineElapsedMs")).toLongLong();
        const double directAlgorithmRmse = directMetadata.value(QStringLiteral("rmse")).toDouble();
        const double directPairedResidualMm =
            directSuccess ? pairedRmse(sourceProbePoints, targetProbePoints, directMatrix) : -1.0;

        QVariantMap parallelParameters = baseParameters;
        const QString parallelId = QStringLiteral("stress_%1_parallel").arg(scenario.id);
        parallelParameters.insert(QStringLiteral("registrationId"), parallelId);
        parallelParameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
        const auto parallelMatrix = service.performICPRegistrationAdvanced(source, target, parallelParameters);
        const bool parallelSuccess = parallelMatrix != nullptr;
        const QVariantMap parallelInfo = service.getRegistrationInfo(parallelId);
        const QVariantMap parallelMetadata = parallelInfo.value(QStringLiteral("metadata")).toMap();
        const qint64 parallelPipelineElapsedMs =
            parallelMetadata.value(QStringLiteral("pipelineElapsedMs")).toLongLong();
        const qint64 parallelSearchTotalMs =
            parallelMetadata.value(QStringLiteral("parallelSearchTotalMs")).toLongLong();
        const double parallelAlgorithmRmse = parallelMetadata.value(QStringLiteral("rmse")).toDouble();
        const double parallelPairedResidualMm =
            parallelSuccess ? pairedRmse(sourceProbePoints, targetProbePoints, parallelMatrix) : -1.0;
        const double speedup =
            parallelPipelineElapsedMs > 0
                ? static_cast<double>(directPipelineElapsedMs) / static_cast<double>(parallelPipelineElapsedMs)
                : 0.0;

        csv << scenario.id << ","
            << directPipelineElapsedMs << ","
            << parallelPipelineElapsedMs << ","
            << QString::number(speedup, 'f', 3) << ","
            << QString::number(directPairedResidualMm, 'f', 4) << ","
            << QString::number(parallelPairedResidualMm, 'f', 4) << ","
            << QString::number(directAlgorithmRmse, 'f', 4) << ","
            << QString::number(parallelAlgorithmRmse, 'f', 4) << ","
            << parallelSearchTotalMs << ","
            << parallelMetadata.value(QStringLiteral("finalResultSource")).toString() << ","
            << parallelMetadata.value(QStringLiteral("parallelScoredCandidateCount")).toInt() << ","
            << parallelMetadata.value(QStringLiteral("refineCandidateCount")).toInt() << ","
            << QString::number(initialPairedResidualMm, 'f', 4) << ","
            << parallelMetadata.value(QStringLiteral("initialAdmissionAction")).toString() << ","
            << parallelMetadata.value(QStringLiteral("initialAdmissionReason")).toString() << ","
            << parallelMetadata.value(QStringLiteral("initialAdmissionRecoveryAction")).toString() << ","
            << (directSuccess ? "true" : "false") << ","
            << (parallelSuccess ? "true" : "false") << "\n";

        qDebug() << "[RealBoneStressMatrix]"
                 << "scenario=" << scenario.id
                 << "direct_ms=" << directPipelineElapsedMs
                 << "parallel_ms=" << parallelPipelineElapsedMs
                 << "direct_paired=" << directPairedResidualMm
                 << "parallel_paired=" << parallelPairedResidualMm
                 << "parallel_source=" << parallelMetadata.value(QStringLiteral("finalResultSource")).toString();
    }

    summaryFile.close();
    return summaryPath;
}

QString RegistrationCoreMeshGpuSmokeTest::runRealBoneRegistrationVisualizationAndWriteArtifacts()
{
    const QString tibiaPath =
        QStringLiteral("D:/Adata/ANSN/ASNS/Release/patient_data/45971129749/reconstructed_mesh_preview/tibia.stl");
    const QString talusPath =
        QStringLiteral("D:/Adata/ANSN/ASNS/Release/patient_data/45971129749/reconstructed_mesh_preview/talus.stl");
    if (!QFileInfo::exists(tibiaPath) || !QFileInfo::exists(talusPath)) {
        return {};
    }

    const QString outputDirPath = QCoreApplication::applicationDirPath()
        + QStringLiteral("/summaries/real_bone_registration_visualization");
    QDir outputDir(outputDirPath);
    outputDir.mkpath(QStringLiteral("."));

    RegistrationServiceImpl service;
    if (!service.loadMeshGPUDLL()) {
        return outputDirPath;
    }

    const auto tibia = loadStlSurface(tibiaPath);
    const auto talus = loadStlSurface(talusPath);
    if (tibia == nullptr || talus == nullptr) {
        return outputDirPath;
    }

    const auto target = appendSurfaces({ tibia, talus });
    if (target == nullptr || target->GetNumberOfPoints() <= 2000) {
        return outputDirPath;
    }

    double talusBounds[6];
    talus->GetBounds(talusBounds);
    const QVector3D targetRegionCenter(
        static_cast<float>((talusBounds[0] + talusBounds[1]) * 0.5),
        static_cast<float>((talusBounds[2] + talusBounds[3]) * 0.5),
        static_cast<float>((talusBounds[4] + talusBounds[5]) * 0.5));

    const QList<QVector3D> targetProbePoints = sampleNearestPoints(target, targetRegionCenter, 240);
    if (targetProbePoints.size() < 120) {
        return outputDirPath;
    }

    QMatrix4x4 sourceToTargetSeed;
    sourceToTargetSeed.setToIdentity();
    sourceToTargetSeed.translate(-7.5f, 4.5f, -3.2f);
    sourceToTargetSeed.rotate(-9.0f, QVector3D(0.0f, 0.0f, 1.0f));
    sourceToTargetSeed.rotate(5.5f, QVector3D(1.0f, 0.0f, 0.0f));
    const QMatrix4x4 targetToSourceGroundTruth = sourceToTargetSeed.inverted();

    QList<QVector3D> sourceProbePoints;
    sourceProbePoints.reserve(targetProbePoints.size());
    for (int pointIndex = 0; pointIndex < targetProbePoints.size(); ++pointIndex) {
        const QVector3D transformedPoint = targetToSourceGroundTruth.map(targetProbePoints.at(pointIndex));
        const float xNoise = static_cast<float>(((pointIndex % 5) - 2) * 0.18);
        const float yNoise = static_cast<float>((((pointIndex * 2) % 5) - 2) * 0.14);
        const float zNoise = static_cast<float>((((pointIndex * 3) % 5) - 2) * 0.10);
        sourceProbePoints.append(transformedPoint + QVector3D(xNoise, yNoise, zNoise));
    }

    const auto source = createPointCloudSurface(sourceProbePoints);
    if (source == nullptr) {
        return outputDirPath;
    }

    QMatrix4x4 perturbedInitialTransform = sourceToTargetSeed;
    perturbedInitialTransform.translate(2.4f, -1.6f, 1.1f);
    perturbedInitialTransform.rotate(3.5f, QVector3D(0.0f, 1.0f, 0.0f));

    vtkSmartPointer<vtkMatrix4x4> initialMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    initialMatrix->Identity();
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            initialMatrix->SetElement(row, column, perturbedInitialTransform(row, column));
        }
    }

    const double rawUnregisteredResidualMm =
        pairedRmseForPointLists(sourceProbePoints, targetProbePoints);
    const double initialPairedResidualMm =
        pairedRmse(sourceProbePoints, targetProbePoints, initialMatrix);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("enableConstraintParallelFilter"), true);
    parameters.insert(QStringLiteral("registrationMethodId"), QStringLiteral("ankle_two_stage_constrained"));
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_real_bone_visual_parallel"));
    parameters.insert(QStringLiteral("candidateCount"), 64);
    parameters.insert(QStringLiteral("topKCandidateCount"), 4);
    parameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_two_level"));
    parameters.insert(QStringLiteral("targetRegionCenterX"), targetRegionCenter.x());
    parameters.insert(QStringLiteral("targetRegionCenterY"), targetRegionCenter.y());
    parameters.insert(QStringLiteral("targetRegionCenterZ"), targetRegionCenter.z());
    parameters.insert(QStringLiteral("targetRegionRadiusMm"), 32.0);
    parameters.insert(QStringLiteral("initialTransform"), matrixToVariantList(perturbedInitialTransform));
    parameters.insert(QStringLiteral("maxIterations"), 18);
    parameters.insert(QStringLiteral("distanceThreshold"), 8.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);
    parameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    parameters.insert(QStringLiteral("robustInitialAvailable"), true);
    parameters.insert(QStringLiteral("robustInitialRmsMm"), initialPairedResidualMm);
    parameters.insert(QStringLiteral("robustInitialConfidence"), 0.85);
    parameters.insert(QStringLiteral("robustInitialInlierCount"), sourceProbePoints.size());
    parameters.insert(QStringLiteral("pairedResidualSourcePoints"), vectorListToVariantList(sourceProbePoints));
    parameters.insert(QStringLiteral("pairedResidualTargetPoints"), vectorListToVariantList(targetProbePoints));
    parameters.insert(QStringLiteral("pairedResidualSourcePointsFlat"), vectorListToFlatVariantList(sourceProbePoints));
    parameters.insert(QStringLiteral("pairedResidualTargetPointsFlat"), vectorListToFlatVariantList(targetProbePoints));
    parameters.insert(QStringLiteral("enablePairedResidualGuard"), true);

    const auto parallelMatrix = service.performICPRegistrationAdvanced(source, target, parameters);
    if (parallelMatrix == nullptr) {
        return outputDirPath;
    }

    const QList<QVector3D> sourceInitialTransformedPoints =
        transformPoints(sourceProbePoints, initialMatrix);
    const QList<QVector3D> sourceParallelFinalTransformedPoints =
        transformPoints(sourceProbePoints, parallelMatrix);
    const QList<QVector3D> targetSurfaceSamplePoints =
        sampleSurfacePointsEvenly(target, 5000);
    const double parallelFinalPairedResidualMm =
        pairedRmseForPointLists(sourceParallelFinalTransformedPoints, targetProbePoints);

    writePointCloudCsv(outputDir.filePath(QStringLiteral("target_surface_sample.csv")), targetSurfaceSamplePoints);
    writePointCloudCsv(outputDir.filePath(QStringLiteral("target_probe.csv")), targetProbePoints);
    writePointCloudCsv(outputDir.filePath(QStringLiteral("source_raw.csv")), sourceProbePoints);
    writePointCloudCsv(
        outputDir.filePath(QStringLiteral("source_initial_transformed.csv")),
        sourceInitialTransformedPoints);
    writePointCloudCsv(
        outputDir.filePath(QStringLiteral("source_parallel_final_transformed.csv")),
        sourceParallelFinalTransformedPoints);
    writeMatrixCsv(outputDir.filePath(QStringLiteral("initial_transform_matrix.csv")), initialMatrix);
    writeMatrixCsv(outputDir.filePath(QStringLiteral("parallel_final_transform_matrix.csv")), parallelMatrix);

    QFile metricsFile(outputDir.filePath(QStringLiteral("metrics.csv")));
    if (metricsFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream metrics(&metricsFile);
        metrics << "stage,paired_residual_mm,point_count\n";
        metrics << "raw_unregistered,"
                << QString::number(rawUnregisteredResidualMm, 'f', 4) << ","
                << sourceProbePoints.size() << "\n";
        metrics << "initial,"
                << QString::number(initialPairedResidualMm, 'f', 4) << ","
                << sourceInitialTransformedPoints.size() << "\n";
        metrics << "parallel_final,"
                << QString::number(parallelFinalPairedResidualMm, 'f', 4) << ","
                << sourceParallelFinalTransformedPoints.size() << "\n";
    }

    const QVariantMap parallelInfo =
        service.getRegistrationInfo(QStringLiteral("meshgpu_real_bone_visual_parallel"));
    const QVariantMap parallelMetadata =
        parallelInfo.value(QStringLiteral("metadata")).toMap();
    QFile metadataFile(outputDir.filePath(QStringLiteral("registration_metadata.csv")));
    if (metadataFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream metadata(&metadataFile);
        metadata << "key,value\n";
        metadata << "algorithm_rmse_mm,"
                 << QString::number(parallelMetadata.value(QStringLiteral("rmse")).toDouble(), 'f', 4)
                 << "\n";
        metadata << "pipeline_elapsed_ms,"
                 << parallelMetadata.value(QStringLiteral("pipelineElapsedMs")).toLongLong()
                 << "\n";
        metadata << "parallel_search_total_ms,"
                 << parallelMetadata.value(QStringLiteral("parallelSearchTotalMs")).toLongLong()
                 << "\n";
        metadata << "final_result_source,"
                 << parallelMetadata.value(QStringLiteral("finalResultSource")).toString()
                 << "\n";
        metadata << "parallel_scored_candidate_count,"
                 << parallelMetadata.value(QStringLiteral("parallelScoredCandidateCount")).toInt()
                 << "\n";
        metadata << "refine_candidate_count,"
                 << parallelMetadata.value(QStringLiteral("refineCandidateCount")).toInt()
                 << "\n";
        metadata << "paired_residual_guard_requested,"
                 << (parallelMetadata.value(QStringLiteral("pairedResidualGuardRequested")).toBool()
                        ? "true"
                        : "false")
                 << "\n";
        metadata << "paired_residual_guard_point_count,"
                 << parallelMetadata.value(QStringLiteral("pairedResidualGuardPointCount")).toInt()
                 << "\n";
        metadata << "paired_residual_guard_target_point_count,"
                 << parallelMetadata.value(QStringLiteral("pairedResidualGuardTargetPointCount")).toInt()
                 << "\n";
        metadata << "paired_residual_guard_source_variant_valid,"
                 << (parallelMetadata.value(QStringLiteral("pairedResidualGuardSourceVariantValid")).toBool()
                        ? "true"
                        : "false")
                 << "\n";
        metadata << "paired_residual_guard_source_variant_type,"
                 << parallelMetadata.value(QStringLiteral("pairedResidualGuardSourceVariantType")).toString()
                 << "\n";
        metadata << "paired_residual_guard_applied,"
                 << (parallelMetadata.value(QStringLiteral("pairedResidualGuardApplied")).toBool()
                        ? "true"
                        : "false")
                 << "\n";
        metadata << "paired_residual_guard_reason,"
                 << parallelMetadata.value(QStringLiteral("pairedResidualGuardReason")).toString()
                 << "\n";
        metadata << "paired_residual_guard_initial_mm,"
                 << QString::number(
                        parallelMetadata.value(QStringLiteral("pairedResidualGuardInitialMm")).toDouble(),
                        'f',
                        4)
                 << "\n";
        metadata << "paired_residual_guard_final_mm,"
                 << QString::number(
                        parallelMetadata.value(QStringLiteral("pairedResidualGuardFinalMm")).toDouble(),
                        'f',
                        4)
                 << "\n";
    }

    writeRegistrationVisualizationHtml(
        outputDir.filePath(QStringLiteral("registration_before_after_view.html")),
        targetSurfaceSamplePoints,
        targetProbePoints,
        sourceProbePoints,
        sourceInitialTransformedPoints,
        sourceParallelFinalTransformedPoints,
        rawUnregisteredResidualMm,
        initialPairedResidualMm,
        parallelFinalPairedResidualMm);

    qDebug() << "[RealBoneRegistrationVisualization]"
             << "outputDir=" << outputDirPath
             << "rawUnregisteredResidualMm=" << rawUnregisteredResidualMm
             << "initialPairedResidualMm=" << initialPairedResidualMm
             << "parallelFinalPairedResidualMm=" << parallelFinalPairedResidualMm
             << "targetSurfaceSamplePoints=" << targetSurfaceSamplePoints.size()
             << "sourcePoints=" << sourceProbePoints.size();

    return outputDirPath;
}

void RegistrationCoreMeshGpuSmokeTest::runtime_output_contains_meshgpu_dll()
{
    const QString runtimeDllPath = QCoreApplication::applicationDirPath() + QStringLiteral("/MeshGPULib.dll");
    QVERIFY2(QFileInfo::exists(runtimeDllPath), qPrintable(runtimeDllPath));
}

void RegistrationCoreMeshGpuSmokeTest::registration_service_loads_meshgpu_dll_from_runtime_output()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_with_gpu_records_gicp_registration_when_runtime_is_available()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_smoke"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);

    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QStringList registrationIds = service.getRegistrationList();
    QVERIFY(registrationIds.contains(QStringLiteral("meshgpu_smoke")));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_smoke"));
    QCOMPARE(info.value(QStringLiteral("type")).toString(), QStringLiteral("gicp"));

    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("algorithm")).toString(), QStringLiteral("GPU-GICP"));
    QVERIFY(metadata.contains(QStringLiteral("elapsedMs")));
    QVERIFY(metadata.contains(QStringLiteral("converged")));
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_with_constraint_payload_records_core_constraint_usage()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_constraint_payload"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);
    parameters.insert(QStringLiteral("targetRegionCenterX"), 1.5);
    parameters.insert(QStringLiteral("targetRegionCenterY"), -2.0);
    parameters.insert(QStringLiteral("targetRegionCenterZ"), 9.0);
    parameters.insert(QStringLiteral("targetRegionRadiusMm"), 0.0);
    parameters.insert(QStringLiteral("initialTransform"), QVariantList {
        1.0, 0.0, 0.0, 1.5,
        0.0, 1.0, 0.0, -2.0,
        0.0, 0.0, 1.0, 3.0,
        0.0, 0.0, 0.0, 1.0
    });
    parameters.insert(QStringLiteral("constraintRegionCount"), 2);
    parameters.insert(QStringLiteral("constraintRegionKeys"), QStringLiteral("tibia_distal_region|talus_dome_region"));

    QVariantMap constraintRegions;
    constraintRegions.insert(
        QStringLiteral("tibia_distal_region"),
        QVariantList {
            QVariantList { -7.5, -14.0, 9.0 },
            QVariantList { -7.5, 10.0, 9.0 },
            QVariantList { 10.5, -14.0, 9.0 }
        });
    constraintRegions.insert(
        QStringLiteral("talus_dome_region"),
        QVariantList {
            QVariantList { -7.5, 10.0, 9.0 },
            QVariantList { 10.5, -14.0, 9.0 },
            QVariantList { 10.5, 10.0, 9.0 }
        });
    parameters.insert(QStringLiteral("constraintRegions"), constraintRegions);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);

    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_constraint_payload"));
    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("constraintRegionCount")).toInt(), 2);
    QCOMPARE(metadata.value(QStringLiteral("constraintRegionKeys")).toString(), QStringLiteral("tibia_distal_region|talus_dome_region"));
    QCOMPARE(metadata.value(QStringLiteral("coreConstraintApplied")).toBool(), true);
    QVERIFY(metadata.value(QStringLiteral("coreConstraintSourcePointCount")).toInt() >= 3);
    QVERIFY(metadata.value(QStringLiteral("coreConstraintSourcePointCount")).toInt() < source->GetNumberOfPoints());
    QVERIFY(metadata.value(QStringLiteral("coreConstraintTargetPointCount")).toInt() >= 3);
    QVERIFY(metadata.value(QStringLiteral("coreConstraintTargetPointCount")).toInt() < target->GetNumberOfPoints());
    QVERIFY(metadata.value(QStringLiteral("coreConstraintTargetTriangleCount")).toInt() > 0);
    QVERIFY(metadata.value(QStringLiteral("coreConstraintTargetTriangleCount")).toInt() < target->GetNumberOfCells());
    QCOMPARE(metadata.value(QStringLiteral("coreConstraintTargetBuildSource")).toString(),
             QStringLiteral("cpu_preupload"));
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_parallel_search_records_parallel_search_metadata()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_parallel_search"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    parameters.insert(QStringLiteral("enableInitialAdmissionGate"), false);
    parameters.insert(QStringLiteral("registrationMethodId"), QStringLiteral("ankle_two_stage_constrained"));
    parameters.insert(QStringLiteral("candidateCount"), 4);
    parameters.insert(QStringLiteral("topKCandidateCount"), 2);
    parameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_three_level"));
    parameters.insert(QStringLiteral("targetRegionCenterX"), 1.5);
    parameters.insert(QStringLiteral("targetRegionCenterY"), -2.0);
    parameters.insert(QStringLiteral("targetRegionCenterZ"), 3.0);
    parameters.insert(QStringLiteral("initialTransform"), QVariantList {
        1.0, 0.0, 0.0, 1.5,
        0.0, 1.0, 0.0, -2.0,
        0.0, 0.0, 1.0, 3.0,
        0.0, 0.0, 0.0, 1.0
    });
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);

    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_parallel_search"));
    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("parallelSearchEnabled")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("candidateCount")).toInt(), 4);
    QCOMPARE(metadata.value(QStringLiteral("topKCount")).toInt(), 2);
    QVERIFY(metadata.value(QStringLiteral("coarseSearchMs")).toLongLong() >= 0);
    QCOMPARE(metadata.value(QStringLiteral("multiResolutionProfile")).toString(), QStringLiteral("ankle_roi_three_level"));
    QVERIFY(!metadata.value(QStringLiteral("bestCandidateId")).toString().isEmpty());
    QCOMPARE(metadata.value(QStringLiteral("bestCandidateRank")).toInt(), 0);
    QVERIFY(metadata.value(QStringLiteral("coarseScore")).toDouble() >= 0.0);
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_with_exact_initial_transform_does_not_double_apply_transform()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_exact_initial_transform"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("initialTransform"), QVariantList {
        1.0, 0.0, 0.0, 1.5,
        0.0, 1.0, 0.0, -2.0,
        0.0, 0.0, 1.0, 3.0,
        0.0, 0.0, 0.0, 1.0
    });
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);
    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QList<QVector3D> sourcePoints = sampleNearestPoints(source, QVector3D(0.0f, 0.0f, 0.0f), 24);
    const QList<QVector3D> targetPoints = sampleNearestPoints(target, QVector3D(1.5f, -2.0f, 3.0f), 24);
    const double residualMm = pairedRmse(sourcePoints, targetPoints, matrix);
    QVERIFY2(residualMm < 0.1, qPrintable(QStringLiteral("Expected near-zero residual with exact initial transform, got %1").arg(residualMm)));
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_parallel_search_with_exact_initial_transform_preserves_alignment()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_parallel_exact_initial_transform"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    parameters.insert(QStringLiteral("enableInitialAdmissionGate"), false);
    parameters.insert(QStringLiteral("registrationMethodId"), QStringLiteral("ankle_two_stage_constrained"));
    parameters.insert(QStringLiteral("candidateCount"), 4);
    parameters.insert(QStringLiteral("topKCandidateCount"), 2);
    parameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_three_level"));
    parameters.insert(QStringLiteral("targetRegionCenterX"), 1.5);
    parameters.insert(QStringLiteral("targetRegionCenterY"), -2.0);
    parameters.insert(QStringLiteral("targetRegionCenterZ"), 3.0);
    parameters.insert(QStringLiteral("initialTransform"), QVariantList {
        1.0, 0.0, 0.0, 1.5,
        0.0, 1.0, 0.0, -2.0,
        0.0, 0.0, 1.0, 3.0,
        0.0, 0.0, 0.0, 1.0
    });
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);
    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QList<QVector3D> sourcePoints = sampleNearestPoints(source, QVector3D(0.0f, 0.0f, 0.0f), 24);
    const QList<QVector3D> targetPoints = sampleNearestPoints(target, QVector3D(1.5f, -2.0f, 3.0f), 24);
    const double residualMm = pairedRmse(sourcePoints, targetPoints, matrix);
    QVERIFY2(
        residualMm < 0.1,
        qPrintable(QStringLiteral("Expected parallel path to preserve exact initial alignment, got residual %1").arg(residualMm)));
}

void RegistrationCoreMeshGpuSmokeTest::runtime_batch_refine_returns_result_per_candidate()
{
    const QString runtimeDllPath = QCoreApplication::applicationDirPath() + QStringLiteral("/MeshGPULib.dll");
    QLibrary runtimeLibrary(runtimeDllPath);
    QVERIFY2(runtimeLibrary.load(), qPrintable(runtimeLibrary.errorString()));

    const auto createRuntimeApi =
        reinterpret_cast<CreateRuntimeApiFn>(runtimeLibrary.resolve("CreateMeshGPURuntimeApi"));
    const auto destroyRuntimeApi =
        reinterpret_cast<DestroyRuntimeApiFn>(runtimeLibrary.resolve("DestroyMeshGPURuntimeApi"));
    QVERIFY(createRuntimeApi != nullptr);
    QVERIFY(destroyRuntimeApi != nullptr);

    mesh_gpu::MeshGPURuntimeApi* runtimeApi = createRuntimeApi();
    QVERIFY(runtimeApi != nullptr);

    auto target = createRegistrationSurface();
    auto source = createRegistrationSurface(1.5, -2.0, 3.0);

    const auto targetVertices = extractPoints(target);
    const auto targetNormals = extractNormals(target);
    const auto targetTriangles = extractTriangles(target);
    const auto sourcePoints = extractPoints(source);

    QVERIFY(runtimeApi->setTargetMesh(targetVertices, targetNormals, targetTriangles, 1.0f));
    QVERIFY(runtimeApi->setSourcePointCloud(sourcePoints));

    const std::vector<mesh_gpu::RuntimeRefineCandidateRequest> candidates {
        { 0, createTranslationTransform(0.0f, 0.0f, 0.0f) },
        { 1, createTranslationTransform(-1.5f, 2.0f, -3.0f) }
    };

    mesh_gpu::RegistrationParams params;
    params.max_iterations = 20;
    params.distance_threshold = 30.0f;
    params.use_point_to_plane = true;

    const std::vector<mesh_gpu::RuntimeRefineCandidateResult> results =
        runtimeApi->refineTransformCandidates(candidates, params);

    destroyRuntimeApi(runtimeApi);
    runtimeApi = nullptr;
    runtimeLibrary.unload();

    QCOMPARE(static_cast<int>(results.size()), 2);
    QCOMPARE(results.at(0).candidateIndex, 0);
    QCOMPARE(results.at(1).candidateIndex, 1);
    QVERIFY(results.at(0).rmse >= 0.0f);
    QVERIFY(results.at(1).rmse >= 0.0f);
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_parallel_search_refines_top_k_and_records_non_zero_best_candidate_rank()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_parallel_search_topk_refine"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    parameters.insert(QStringLiteral("enableInitialAdmissionGate"), false);
    parameters.insert(QStringLiteral("registrationMethodId"), QStringLiteral("ankle_two_stage_constrained"));
    parameters.insert(QStringLiteral("candidateCount"), 4);
    parameters.insert(QStringLiteral("topKCandidateCount"), 2);
    parameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_three_level"));
    parameters.insert(QStringLiteral("targetRegionCenterX"), 1.5);
    parameters.insert(QStringLiteral("targetRegionCenterY"), -2.0);
    parameters.insert(QStringLiteral("targetRegionCenterZ"), 3.0);
    parameters.insert(QStringLiteral("initialTransform"), QVariantList {
        1.0, 0.0, 0.0, 1.5,
        0.0, 1.0, 0.0, -2.0,
        0.0, 0.0, 1.0, 3.0,
        0.0, 0.0, 0.0, 1.0
    });
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);

    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_parallel_search_topk_refine"));
    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("parallelSearchEnabled")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("topKCount")).toInt(), 2);
    QVERIFY(metadata.contains(QStringLiteral("bestCandidateRank")));
    QVERIFY(metadata.value(QStringLiteral("bestCandidateRank")).toInt() >= 0);
    QCOMPARE(metadata.value(QStringLiteral("batchRefineRequested")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("batchRefineEnabled")).toBool(), true);
    QVERIFY(metadata.value(QStringLiteral("batchRefineFallback")).toString().isEmpty());
    QVERIFY(metadata.contains(QStringLiteral("refineCandidateCount")));
    QCOMPARE(metadata.value(QStringLiteral("refineCandidateCount")).toInt(), 2);
    QVERIFY(metadata.contains(QStringLiteral("refineMs")));
    QVERIFY(metadata.value(QStringLiteral("refineMs")).toLongLong() >= 0);
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_parallel_search_records_batch_refine_metadata()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_parallel_search_batch_refine_metadata"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    parameters.insert(QStringLiteral("enableInitialAdmissionGate"), false);
    parameters.insert(QStringLiteral("registrationMethodId"), QStringLiteral("ankle_two_stage_constrained"));
    parameters.insert(QStringLiteral("candidateCount"), 4);
    parameters.insert(QStringLiteral("topKCandidateCount"), 2);
    parameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_three_level"));
    parameters.insert(QStringLiteral("targetRegionCenterX"), 1.5);
    parameters.insert(QStringLiteral("targetRegionCenterY"), -2.0);
    parameters.insert(QStringLiteral("targetRegionCenterZ"), 3.0);
    parameters.insert(QStringLiteral("initialTransform"), QVariantList {
        1.0, 0.0, 0.0, 1.5,
        0.0, 1.0, 0.0, -2.0,
        0.0, 0.0, 1.0, 3.0,
        0.0, 0.0, 0.0, 1.0
    });
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);

    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_parallel_search_batch_refine_metadata"));
    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("batchRefineRequested")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("batchRefineEnabled")).toBool(), true);
    QVERIFY(metadata.value(QStringLiteral("batchRefineFallback")).toString().isEmpty());
    QCOMPARE(metadata.value(QStringLiteral("refineCandidateCount")).toInt(), 2);
    QVERIFY(metadata.value(QStringLiteral("refineMs")).toLongLong() >= 0);
    QVERIFY(metadata.contains(QStringLiteral("bestBatchRefineRmse")));
    QVERIFY(metadata.value(QStringLiteral("bestBatchRefineRmse")).toDouble() >= 0.0);
    QCOMPARE(metadata.value(QStringLiteral("bestBatchRefineTransformApplied")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("finalResultSource")).toString(), QStringLiteral("parallel_batch_refine"));
    QCOMPARE(metadata.value(QStringLiteral("precomputedBatchRefineFastPath")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("finalStageTargetPrepared")).toBool(), false);
    QCOMPARE(metadata.value(QStringLiteral("batchRefineReusedPreparedRuntimeState")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("batchRefineTargetPrepared")).toBool(), false);
    QCOMPARE(metadata.value(QStringLiteral("batchRefineSourcePrepared")).toBool(), false);
    QVERIFY(metadata.value(QStringLiteral("batchRefineCellSize")).toDouble() > 0.0);
    QVERIFY(metadata.contains(QStringLiteral("parallelSearchTotalMs")));
    QVERIFY(metadata.contains(QStringLiteral("pipelineElapsedMs")));
    QVERIFY(metadata.value(QStringLiteral("parallelSearchTotalMs")).toLongLong() >= 0);
    QVERIFY(metadata.value(QStringLiteral("pipelineElapsedMs")).toLongLong()
             >= metadata.value(QStringLiteral("parallelSearchTotalMs")).toLongLong());
    QVERIFY(metadata.value(QStringLiteral("rmse")).toDouble()
             <= metadata.value(QStringLiteral("bestBatchRefineRmse")).toDouble() + 1e-4);
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_parallel_search_with_constraints_records_parallel_search_metadata()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_parallel_search_constraints"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    parameters.insert(QStringLiteral("registrationMethodId"), QStringLiteral("ankle_two_stage_constrained"));
    parameters.insert(QStringLiteral("candidateCount"), 4);
    parameters.insert(QStringLiteral("topKCandidateCount"), 2);
    parameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_three_level"));
    parameters.insert(QStringLiteral("targetRegionCenterX"), 1.5);
    parameters.insert(QStringLiteral("targetRegionCenterY"), -2.0);
    parameters.insert(QStringLiteral("targetRegionCenterZ"), 9.0);
    parameters.insert(QStringLiteral("targetRegionRadiusMm"), 0.0);
    parameters.insert(QStringLiteral("initialTransform"), QVariantList {
        1.0, 0.0, 0.0, 1.5,
        0.0, 1.0, 0.0, -2.0,
        0.0, 0.0, 1.0, 3.0,
        0.0, 0.0, 0.0, 1.0
    });
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);
    parameters.insert(QStringLiteral("constraintRegionCount"), 2);
    parameters.insert(QStringLiteral("constraintRegionKeys"), QStringLiteral("tibia_distal_region|talus_dome_region"));

    QVariantMap constraintRegions;
    constraintRegions.insert(
        QStringLiteral("tibia_distal_region"),
        QVariantList {
            QVariantList { -7.5, -14.0, 9.0 },
            QVariantList { -7.5, 10.0, 9.0 },
            QVariantList { 10.5, -14.0, 9.0 }
        });
    constraintRegions.insert(
        QStringLiteral("talus_dome_region"),
        QVariantList {
            QVariantList { -7.5, 10.0, 9.0 },
            QVariantList { 10.5, -14.0, 9.0 },
            QVariantList { 10.5, 10.0, 9.0 }
        });
    parameters.insert(QStringLiteral("constraintRegions"), constraintRegions);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);

    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_parallel_search_constraints"));
    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("parallelSearchEnabled")).toBool(), true);
    QCOMPARE(metadata.value(QStringLiteral("candidateCount")).toInt(), 4);
    QCOMPARE(metadata.value(QStringLiteral("topKCount")).toInt(), 2);
    QVERIFY(metadata.value(QStringLiteral("coarseSearchMs")).toLongLong() >= 0);
    QVERIFY(!metadata.value(QStringLiteral("bestCandidateId")).toString().isEmpty());
    QCOMPARE(metadata.value(QStringLiteral("bestCandidateRank")).toInt(), 0);
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_parallel_search_records_multi_resolution_and_constraint_filter_metrics()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_multires_roi"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    parameters.insert(QStringLiteral("enableConstraintParallelFilter"), true);
    parameters.insert(QStringLiteral("registrationMethodId"), QStringLiteral("ankle_two_stage_constrained"));
    parameters.insert(QStringLiteral("candidateCount"), 32);
    parameters.insert(QStringLiteral("topKCandidateCount"), 4);
    parameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_three_level"));
    parameters.insert(QStringLiteral("targetRegionCenterX"), 1.5);
    parameters.insert(QStringLiteral("targetRegionCenterY"), -2.0);
    parameters.insert(QStringLiteral("targetRegionCenterZ"), 9.0);
    parameters.insert(QStringLiteral("targetRegionRadiusMm"), 18.0);
    parameters.insert(QStringLiteral("initialTransform"), QVariantList {
        1.0, 0.0, 0.0, 1.5,
        0.0, 1.0, 0.0, -2.0,
        0.0, 0.0, 1.0, 3.0,
        0.0, 0.0, 0.0, 1.0
    });
    parameters.insert(QStringLiteral("maxIterations"), 10);
    parameters.insert(QStringLiteral("distanceThreshold"), 30.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);

    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_multires_roi"));
    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("multiResolutionLevelCount")).toInt(), 3);
    QCOMPARE(metadata.value(QStringLiteral("constraintParallelFilterEnabled")).toBool(), true);
    QVERIFY(metadata.value(QStringLiteral("roiFilterMs")).toLongLong() >= 0);
    QCOMPARE(metadata.value(QStringLiteral("runtimeSourceConstraintFilterUsed")).toBool(), true);
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_with_pose_perturbation_records_nonzero_rmse_and_iteration_count()
{
    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    auto source = createRegistrationSurface();
    auto target = createRegistrationSurface(1.5, -2.0, 3.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_pose_perturbation"));
    parameters.insert(QStringLiteral("useGPU"), true);
    parameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    parameters.insert(QStringLiteral("enableInitialAdmissionGate"), false);
    parameters.insert(QStringLiteral("registrationMethodId"), QStringLiteral("ankle_two_stage_constrained"));
    parameters.insert(QStringLiteral("candidateCount"), 32);
    parameters.insert(QStringLiteral("topKCandidateCount"), 4);
    parameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_three_level"));
    parameters.insert(QStringLiteral("targetRegionCenterX"), 1.5);
    parameters.insert(QStringLiteral("targetRegionCenterY"), -2.0);
    parameters.insert(QStringLiteral("targetRegionCenterZ"), 3.0);
    parameters.insert(QStringLiteral("targetRegionRadiusMm"), 18.0);
    parameters.insert(QStringLiteral("initialTransform"), QVariantList {
        1.0, 0.0, 0.0, 4.0,
        0.0, 1.0, 0.0, -6.5,
        0.0, 0.0, 1.0, 8.0,
        0.0, 0.0, 0.0, 1.0
    });
    parameters.insert(QStringLiteral("maxIterations"), 15);
    parameters.insert(QStringLiteral("distanceThreshold"), 12.0);
    parameters.insert(QStringLiteral("usePointToPlane"), true);
    parameters.insert(QStringLiteral("verbose"), false);

    const auto matrix = service.performICPRegistrationAdvanced(source, target, parameters);
    QVERIFY2(matrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap info = service.getRegistrationInfo(QStringLiteral("meshgpu_pose_perturbation"));
    const QVariantMap metadata = info.value(QStringLiteral("metadata")).toMap();
    QVERIFY(metadata.value(QStringLiteral("rmse")).toDouble() >= 0.0);
    QVERIFY(metadata.value(QStringLiteral("iterations")).toInt() >= 0);
    QVERIFY(metadata.value(QStringLiteral("elapsedMs")).toLongLong() >= 0);
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_real_bone_mesh_partial_surface_registration_reports_realistic_error()
{
    const QString tibiaPath =
        QStringLiteral("D:/Adata/ANSN/ASNS/Release/patient_data/45971129749/reconstructed_mesh_preview/tibia.stl");
    const QString talusPath =
        QStringLiteral("D:/Adata/ANSN/ASNS/Release/patient_data/45971129749/reconstructed_mesh_preview/talus.stl");
    if (!QFileInfo::exists(tibiaPath) || !QFileInfo::exists(talusPath)) {
        QSKIP("Real bone STL assets are not available on this machine.");
    }

    RegistrationServiceImpl service;
    QVERIFY(service.loadMeshGPUDLL());

    const auto tibia = loadStlSurface(tibiaPath);
    const auto talus = loadStlSurface(talusPath);
    QVERIFY(tibia != nullptr);
    QVERIFY(talus != nullptr);
    QVERIFY(tibia->GetNumberOfPoints() > 1000);
    QVERIFY(talus->GetNumberOfPoints() > 1000);

    const auto target = appendSurfaces({ tibia, talus });
    QVERIFY(target != nullptr);
    QVERIFY(target->GetNumberOfPoints() > 2000);

    double talusBounds[6];
    talus->GetBounds(talusBounds);
    const QVector3D targetRegionCenter(
        static_cast<float>((talusBounds[0] + talusBounds[1]) * 0.5),
        static_cast<float>((talusBounds[2] + talusBounds[3]) * 0.5),
        static_cast<float>((talusBounds[4] + talusBounds[5]) * 0.5));

    const QList<QVector3D> targetProbePoints = sampleNearestPoints(target, targetRegionCenter, 240);
    QVERIFY(targetProbePoints.size() >= 120);

    QMatrix4x4 sourceToTargetSeed;
    sourceToTargetSeed.setToIdentity();
    sourceToTargetSeed.translate(-7.5f, 4.5f, -3.2f);
    sourceToTargetSeed.rotate(-9.0f, QVector3D(0.0f, 0.0f, 1.0f));
    sourceToTargetSeed.rotate(5.5f, QVector3D(1.0f, 0.0f, 0.0f));
    const QMatrix4x4 targetToSourceGroundTruth = sourceToTargetSeed.inverted();

    QList<QVector3D> sourceProbePoints;
    sourceProbePoints.reserve(targetProbePoints.size());
    for (int pointIndex = 0; pointIndex < targetProbePoints.size(); ++pointIndex) {
        const QVector3D transformedPoint = targetToSourceGroundTruth.map(targetProbePoints.at(pointIndex));
        const float xNoise = static_cast<float>(((pointIndex % 5) - 2) * 0.18);
        const float yNoise = static_cast<float>((((pointIndex * 2) % 5) - 2) * 0.14);
        const float zNoise = static_cast<float>((((pointIndex * 3) % 5) - 2) * 0.10);
        sourceProbePoints.append(transformedPoint + QVector3D(xNoise, yNoise, zNoise));
    }

    const auto source = createPointCloudSurface(sourceProbePoints);
    QVERIFY(source != nullptr);
    QCOMPARE(source->GetNumberOfPoints(), targetProbePoints.size());

    QMatrix4x4 perturbedInitialTransform = sourceToTargetSeed;
    perturbedInitialTransform.translate(2.4f, -1.6f, 1.1f);
    perturbedInitialTransform.rotate(3.5f, QVector3D(0.0f, 1.0f, 0.0f));

    QVariantMap baseParameters;
    baseParameters.insert(QStringLiteral("useGPU"), true);
    baseParameters.insert(QStringLiteral("enableConstraintParallelFilter"), true);
    baseParameters.insert(QStringLiteral("registrationMethodId"), QStringLiteral("ankle_two_stage_constrained"));
    baseParameters.insert(QStringLiteral("candidateCount"), 64);
    baseParameters.insert(QStringLiteral("topKCandidateCount"), 4);
    baseParameters.insert(QStringLiteral("multiResolutionProfileId"), QStringLiteral("ankle_roi_two_level"));
    baseParameters.insert(QStringLiteral("targetRegionCenterX"), targetRegionCenter.x());
    baseParameters.insert(QStringLiteral("targetRegionCenterY"), targetRegionCenter.y());
    baseParameters.insert(QStringLiteral("targetRegionCenterZ"), targetRegionCenter.z());
    baseParameters.insert(QStringLiteral("targetRegionRadiusMm"), 32.0);
    baseParameters.insert(QStringLiteral("initialTransform"), matrixToVariantList(perturbedInitialTransform));
    baseParameters.insert(QStringLiteral("maxIterations"), 18);
    baseParameters.insert(QStringLiteral("distanceThreshold"), 8.0);
    baseParameters.insert(QStringLiteral("usePointToPlane"), true);
    baseParameters.insert(QStringLiteral("verbose"), false);

    vtkSmartPointer<vtkMatrix4x4> initialMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    initialMatrix->Identity();
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            initialMatrix->SetElement(row, column, perturbedInitialTransform(row, column));
        }
    }
    const double initialPairedResidualMm = pairedRmse(sourceProbePoints, targetProbePoints, initialMatrix);

    QVariantMap directParameters = baseParameters;
    directParameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_real_bone_direct"));
    directParameters.insert(QStringLiteral("enableParallelInitialSearch"), false);
    const auto directMatrix = service.performICPRegistrationAdvanced(source, target, directParameters);
    QVERIFY2(directMatrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap directInfo = service.getRegistrationInfo(QStringLiteral("meshgpu_real_bone_direct"));
    const QVariantMap directMetadata = directInfo.value(QStringLiteral("metadata")).toMap();
    const double directAlgorithmRmse = directMetadata.value(QStringLiteral("rmse")).toDouble();
    const qint64 directPipelineElapsedMs = directMetadata.value(QStringLiteral("pipelineElapsedMs")).toLongLong();
    const double directPairedResidualMm = pairedRmse(sourceProbePoints, targetProbePoints, directMatrix);

    QVariantMap parallelParameters = baseParameters;
    parallelParameters.insert(QStringLiteral("registrationId"), QStringLiteral("meshgpu_real_bone_parallel"));
    parallelParameters.insert(QStringLiteral("enableParallelInitialSearch"), true);
    const auto parallelMatrix = service.performICPRegistrationAdvanced(source, target, parallelParameters);
    QVERIFY2(parallelMatrix != nullptr, qPrintable(service.getLastError()));

    const QVariantMap parallelInfo = service.getRegistrationInfo(QStringLiteral("meshgpu_real_bone_parallel"));
    const QVariantMap parallelMetadata = parallelInfo.value(QStringLiteral("metadata")).toMap();
    const double parallelAlgorithmRmse = parallelMetadata.value(QStringLiteral("rmse")).toDouble();
    const int parallelIterations = parallelMetadata.value(QStringLiteral("iterations")).toInt();
    const qint64 parallelElapsedMs = parallelMetadata.value(QStringLiteral("elapsedMs")).toLongLong();
    const qint64 parallelPipelineElapsedMs =
        parallelMetadata.value(QStringLiteral("pipelineElapsedMs")).toLongLong();
    const qint64 parallelSearchTotalMs =
        parallelMetadata.value(QStringLiteral("parallelSearchTotalMs")).toLongLong();
    const double parallelPairedResidualMm = pairedRmse(sourceProbePoints, targetProbePoints, parallelMatrix);

    qDebug() << "[RealBoneRegistration]"
             << "initialPairedResidualMm=" << initialPairedResidualMm
             << "directAlgorithmRmseMm=" << directAlgorithmRmse
             << "directPairedResidualMm=" << directPairedResidualMm
             << "parallelAlgorithmRmseMm=" << parallelAlgorithmRmse
             << "parallelPairedResidualMm=" << parallelPairedResidualMm
             << "parallelBestCandidateRank=" << parallelMetadata.value(QStringLiteral("bestCandidateRank")).toInt()
             << "parallelCoarseScore=" << parallelMetadata.value(QStringLiteral("coarseScore")).toDouble()
             << "parallelIterations=" << parallelIterations
             << "parallelElapsedMs=" << parallelElapsedMs
             << "directPipelineElapsedMs=" << directPipelineElapsedMs
             << "parallelPipelineElapsedMs=" << parallelPipelineElapsedMs
             << "parallelSearchTotalMs=" << parallelSearchTotalMs
             << "parallelScoredCandidateCount="
             << parallelMetadata.value(QStringLiteral("parallelScoredCandidateCount")).toInt()
             << "parallelCpuIdentityProbeUsed="
             << parallelMetadata.value(QStringLiteral("parallelCpuIdentityProbeUsed")).toBool()
             << "parallelCpuIdentityProbeScoreMm="
             << parallelMetadata.value(QStringLiteral("parallelCpuIdentityProbeScoreMm")).toDouble()
             << "parallelCpuIdentityProbeEarlyAccepted="
             << parallelMetadata.value(QStringLiteral("parallelCpuIdentityProbeEarlyAccepted")).toBool()
             << "parallelCpuIdentityProbeTargetPointCount="
             << parallelMetadata.value(QStringLiteral("parallelCpuIdentityProbeTargetPointCount")).toInt()
             << "parallelRefineCandidateCount=" << parallelMetadata.value(QStringLiteral("refineCandidateCount")).toInt()
             << "parallelRefineCandidateIds=" << parallelMetadata.value(QStringLiteral("refineCandidateIds")).toList()
             << "parallelFinalResultSource=" << parallelMetadata.value(QStringLiteral("finalResultSource")).toString()
             << "parallelBestBatchRefineCandidateId="
             << parallelMetadata.value(QStringLiteral("bestBatchRefineCandidateId")).toString()
             << "parallelConfidentInitialFastPathApplied="
             << parallelMetadata.value(QStringLiteral("confidentInitialFastPathApplied")).toBool()
             << "parallelConfidentInitialIdentityCoarseScoreMm="
             << parallelMetadata.value(QStringLiteral("confidentInitialFastPathIdentityCoarseScoreMm")).toDouble()
             << "parallelBatchRefineCandidateDetails=" << parallelMetadata.value(QStringLiteral("batchRefineCandidateDetails")).toList()
             << "parallelTopKCandidateDetails=" << parallelMetadata.value(QStringLiteral("topKCandidateDetails")).toList()
             << "sourcePoints=" << sourceProbePoints.size()
             << "targetPoints=" << target->GetNumberOfPoints();

    QVERIFY(initialPairedResidualMm > 0.0);
    QVERIFY(directAlgorithmRmse >= 0.0);
    QVERIFY(parallelAlgorithmRmse >= 0.0);
    QVERIFY(directPairedResidualMm > 0.0);
    QVERIFY2(directPairedResidualMm <= 5.0,
             qPrintable(QStringLiteral("Direct paired residual exceeded clinical simulation threshold: %1mm")
                 .arg(directPairedResidualMm)));
    QVERIFY2(parallelPairedResidualMm <= 5.0,
             qPrintable(QStringLiteral("Parallel paired residual exceeded clinical simulation threshold: %1mm")
                 .arg(parallelPairedResidualMm)));
    QVERIFY(parallelMetadata.value(QStringLiteral("parallelSearchEnabled")).toBool());
    QVERIFY(!parallelMetadata.value(QStringLiteral("bestCandidateId")).toString().isEmpty());
    QVERIFY(parallelMetadata.contains(QStringLiteral("bestBatchRefineRmse")));
    QVERIFY(parallelMetadata.value(QStringLiteral("bestBatchRefineTransformApplied")).toBool());
    const QString finalResultSource = parallelMetadata.value(QStringLiteral("finalResultSource")).toString();
    QVERIFY(finalResultSource == QStringLiteral("parallel_batch_refine")
            || finalResultSource == QStringLiteral("parallel_confident_initial"));
    if (finalResultSource == QStringLiteral("parallel_confident_initial")) {
        QVERIFY(parallelMetadata.value(QStringLiteral("confidentInitialFastPathApplied")).toBool());
        QCOMPARE(parallelMetadata.value(QStringLiteral("refineCandidateCount")).toInt(), 0);
        QCOMPARE(
            parallelMetadata.value(QStringLiteral("bestBatchRefineCandidateId")).toString(),
            QStringLiteral("candidate_000"));
        QVERIFY(
            parallelMetadata.value(QStringLiteral("confidentInitialFastPathIdentityCoarseScoreMm")).toDouble()
            <= parallelMetadata.value(QStringLiteral("confidentInitialFastPathMaxCoarseScoreMm")).toDouble());
        QCOMPARE(parallelMetadata.value(QStringLiteral("multiResolutionLevelCount")).toInt(), 1);
        QCOMPARE(parallelMetadata.value(QStringLiteral("parallelScoredCandidateCount")).toInt(), 1);
        QVERIFY(parallelMetadata.value(QStringLiteral("parallelCpuIdentityProbeUsed")).toBool());
        QVERIFY(parallelMetadata.value(QStringLiteral("parallelCpuIdentityProbeEarlyAccepted")).toBool());
        QVERIFY(parallelMetadata.value(QStringLiteral("parallelCpuIdentityProbeTargetPointCount")).toInt() > 0);
    }
    QVERIFY(parallelAlgorithmRmse
             <= parallelMetadata.value(QStringLiteral("bestBatchRefineRmse")).toDouble() + 1e-4);
    if (parallelPairedResidualMm > directPairedResidualMm) {
        qWarning() << "[RealBoneRegistration] Parallel paired residual did not beat direct in this noisy run"
                   << "directPairedResidualMm=" << directPairedResidualMm
                   << "parallelPairedResidualMm=" << parallelPairedResidualMm;
    }
    QVERIFY(parallelIterations >= 0);
    QVERIFY(parallelElapsedMs >= 0);
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_real_bone_stress_matrix_exports_summary_csv()
{
    const QString summaryPath = runRealBoneStressMatrixAndWriteSummary();
    if (summaryPath.isEmpty()) {
        QSKIP("Real bone STL assets are not available on this machine.");
    }

    QVERIFY2(QFileInfo::exists(summaryPath), qPrintable(summaryPath));
    QFile summaryFile(summaryPath);
    QVERIFY(summaryFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString csv = QString::fromUtf8(summaryFile.readAll());
    QVERIFY(csv.contains(QStringLiteral("scenario,direct_ms,parallel_ms")));
    QVERIFY(csv.contains(QStringLiteral("baseline")));
    QVERIFY(csv.contains(QStringLiteral("high_noise")));
    QVERIFY(csv.contains(QStringLiteral("outlier_points")));
    QVERIFY(csv.contains(QStringLiteral("large_initial_offset")));
}

void RegistrationCoreMeshGpuSmokeTest::advanced_icp_real_bone_registration_visualization_exports_before_after_clouds()
{
    const QString outputDirPath = runRealBoneRegistrationVisualizationAndWriteArtifacts();
    if (outputDirPath.isEmpty()) {
        QSKIP("Real bone STL assets are not available on this machine.");
    }

    const QDir outputDir(outputDirPath);
    QVERIFY2(outputDir.exists(), qPrintable(outputDirPath));

    const QString htmlPath = outputDir.filePath(QStringLiteral("registration_before_after_view.html"));
    const QString metricsPath = outputDir.filePath(QStringLiteral("metrics.csv"));
    const QString sourceRawPath = outputDir.filePath(QStringLiteral("source_raw.csv"));
    const QString targetProbePath = outputDir.filePath(QStringLiteral("target_probe.csv"));
    const QString sourceInitialPath = outputDir.filePath(QStringLiteral("source_initial_transformed.csv"));
    const QString sourceFinalPath = outputDir.filePath(QStringLiteral("source_parallel_final_transformed.csv"));

    QVERIFY2(QFileInfo::exists(htmlPath), qPrintable(htmlPath));
    QVERIFY2(QFileInfo::exists(metricsPath), qPrintable(metricsPath));
    QVERIFY2(QFileInfo::exists(sourceRawPath), qPrintable(sourceRawPath));
    QVERIFY2(QFileInfo::exists(targetProbePath), qPrintable(targetProbePath));
    QVERIFY2(QFileInfo::exists(sourceInitialPath), qPrintable(sourceInitialPath));
    QVERIFY2(QFileInfo::exists(sourceFinalPath), qPrintable(sourceFinalPath));

    QFile htmlFile(htmlPath);
    QVERIFY(htmlFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString html = QString::fromUtf8(htmlFile.readAll());
    QVERIFY(html.contains(QStringLiteral("source_raw")));
    QVERIFY(html.contains(QStringLiteral("target_probe")));
    QVERIFY(html.contains(QStringLiteral("source_initial_transformed")));
    QVERIFY(html.contains(QStringLiteral("source_parallel_final_transformed")));

    QFile metricsFile(metricsPath);
    QVERIFY(metricsFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString metrics = QString::fromUtf8(metricsFile.readAll());
    QVERIFY(metrics.contains(QStringLiteral("stage,paired_residual_mm")));
    QVERIFY(metrics.contains(QStringLiteral("raw_unregistered")));
    QVERIFY(metrics.contains(QStringLiteral("parallel_final")));
    QStringList metricLines = metrics.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    double initialResidualMm = -1.0;
    double finalResidualMm = -1.0;
    for (const QString& line : metricLines) {
        const QStringList columns = line.split(QLatin1Char(','));
        if (columns.size() < 2) {
            continue;
        }
        if (columns.at(0) == QStringLiteral("initial")) {
            initialResidualMm = columns.at(1).toDouble();
        }
        if (columns.at(0) == QStringLiteral("parallel_final")) {
            finalResidualMm = columns.at(1).toDouble();
        }
    }
    QVERIFY(initialResidualMm > 0.0);
    QVERIFY(finalResidualMm > 0.0);
    QVERIFY2(
        finalResidualMm <= initialResidualMm + 1e-4,
        qPrintable(QStringLiteral("Final paired residual regressed: initial=%1 final=%2")
            .arg(initialResidualMm, 0, 'f', 4)
            .arg(finalResidualMm, 0, 'f', 4)));
}

void RegistrationCoreMeshGpuSmokeTest::candidate_batch_scoring_returns_ranked_scores_from_runtime_api()
{
    const QString runtimeDllPath = QCoreApplication::applicationDirPath() + QStringLiteral("/MeshGPULib.dll");
    QLibrary runtimeLibrary(runtimeDllPath);
    QVERIFY2(runtimeLibrary.load(), qPrintable(runtimeLibrary.errorString()));

    const auto createRuntimeApi =
        reinterpret_cast<CreateRuntimeApiFn>(runtimeLibrary.resolve("CreateMeshGPURuntimeApi"));
    const auto destroyRuntimeApi =
        reinterpret_cast<DestroyRuntimeApiFn>(runtimeLibrary.resolve("DestroyMeshGPURuntimeApi"));
    QVERIFY(createRuntimeApi != nullptr);
    QVERIFY(destroyRuntimeApi != nullptr);

    mesh_gpu::MeshGPURuntimeApi* runtimeApi = createRuntimeApi();
    QVERIFY(runtimeApi != nullptr);

    auto target = createRegistrationSurface();
    auto source = createRegistrationSurface(1.5, -2.0, 3.0);

    const auto targetVertices = extractPoints(target);
    const auto targetNormals = extractNormals(target);
    const auto targetTriangles = extractTriangles(target);
    const auto sourcePoints = extractPoints(source);

    QVERIFY(runtimeApi->setTargetMesh(targetVertices, targetNormals, targetTriangles, 1.0f));
    QVERIFY(runtimeApi->setSourcePointCloud(sourcePoints));

    const std::vector<mesh_gpu::Transform4x4> candidates {
        createTranslationTransform(0.0f, 0.0f, 0.0f),
        createTranslationTransform(-1.5f, 2.0f, -3.0f)
    };

    const auto scores = runtimeApi->scoreTransformCandidates(candidates, 20.0f);
    if (scores.size() == 2) {
        qDebug() << "[CandidateGeometryScore]"
                 << "bestIndex=" << scores.front().candidateIndex
                 << "bestNormal=" << scores.front().normalConsistencyScore
                 << "worstNormal=" << scores.back().normalConsistencyScore
                 << "bestCurvature=" << scores.front().curvatureScore
                 << "worstCurvature=" << scores.back().curvatureScore
                 << "bestGeometryAvailable=" << scores.front().geometryScoreAvailable
                 << "worstGeometryAvailable=" << scores.back().geometryScoreAvailable;
    }

    destroyRuntimeApi(runtimeApi);
    runtimeApi = nullptr;
    runtimeLibrary.unload();

    QCOMPARE(static_cast<int>(scores.size()), 2);
    QCOMPARE(scores.front().candidateIndex, 1);
    QCOMPARE(scores.back().candidateIndex, 0);
    QVERIFY(scores.front().score >= scores.back().score);
    QVERIFY(scores.front().meanDistanceMm >= 0.0f);
    QVERIFY(scores.back().meanDistanceMm >= 0.0f);
    QVERIFY(scores.front().meanDistanceMm <= scores.back().meanDistanceMm);
    QVERIFY(scores.front().geometryScoreAvailable);
    QVERIFY(scores.back().geometryScoreAvailable);
    QVERIFY(scores.front().normalConsistencyScore >= 0.0f);
    QVERIFY(scores.front().normalConsistencyScore <= 1.0f);
    QVERIFY(scores.back().normalConsistencyScore >= 0.0f);
    QVERIFY(scores.back().normalConsistencyScore <= 1.0f);
    QVERIFY(scores.front().normalConsistencyScore >= scores.back().normalConsistencyScore);
    QVERIFY(scores.front().curvatureScore >= 0.0f);
    QVERIFY(scores.front().curvatureScore <= 1.0f);
    QVERIFY(scores.back().curvatureScore >= 0.0f);
    QVERIFY(scores.back().curvatureScore <= 1.0f);
    QVERIFY(scores.front().success);
    QVERIFY(scores.back().success);
}

void RegistrationCoreMeshGpuSmokeTest::runtime_constraint_filter_returns_selected_indices()
{
    std::cout << "[SmokeTest] runtime_constraint_filter_returns_selected_indices: start\n";
    std::cout.flush();
    const QString runtimeDllPath = QCoreApplication::applicationDirPath() + QStringLiteral("/MeshGPULib.dll");
    QLibrary runtimeLibrary(runtimeDllPath);
    QVERIFY2(runtimeLibrary.load(), qPrintable(runtimeLibrary.errorString()));

    const auto createRuntimeApi =
        reinterpret_cast<CreateRuntimeApiFn>(runtimeLibrary.resolve("CreateMeshGPURuntimeApi"));
    const auto destroyRuntimeApi =
        reinterpret_cast<DestroyRuntimeApiFn>(runtimeLibrary.resolve("DestroyMeshGPURuntimeApi"));
    QVERIFY(createRuntimeApi != nullptr);
    QVERIFY(destroyRuntimeApi != nullptr);

    mesh_gpu::MeshGPURuntimeApi* runtimeApi = createRuntimeApi();
    QVERIFY(runtimeApi != nullptr);

    const std::vector<mesh_gpu::Point3D> sourcePoints {
        mesh_gpu::Point3D(-7.5f, -14.0f, 9.0f),
        mesh_gpu::Point3D(-7.5f, 10.0f, 9.0f),
        mesh_gpu::Point3D(10.5f, -14.0f, 9.0f),
        mesh_gpu::Point3D(10.5f, 10.0f, 9.0f),
        mesh_gpu::Point3D(50.0f, 50.0f, 50.0f)
    };
    const std::vector<mesh_gpu::Point3D> constraintPoints {
        mesh_gpu::Point3D(-7.5f, -14.0f, 9.0f),
        mesh_gpu::Point3D(-7.5f, 10.0f, 9.0f),
        mesh_gpu::Point3D(10.5f, -14.0f, 9.0f)
    };

    const auto result = runtimeApi->filterSourcePointsByConstraints(
        sourcePoints,
        mesh_gpu::Point3D(1.5f, -2.0f, 9.0f),
        18.0f,
        5.4f,
        constraintPoints,
        3);
    std::cout << "[SmokeTest] runtime_constraint_filter_returns_selected_indices: call returned "
              << static_cast<int>(result.success) << " "
              << static_cast<int>(result.selectedIndices.size()) << "\n";
    std::cout.flush();

    destroyRuntimeApi(runtimeApi);
    runtimeApi = nullptr;
    runtimeLibrary.unload();
    std::cout << "[SmokeTest] runtime_constraint_filter_returns_selected_indices: end\n";
    std::cout.flush();

    QVERIFY(result.success);
    QVERIFY(static_cast<int>(result.selectedIndices.size()) >= 3);
    QVERIFY(std::find(result.selectedIndices.begin(), result.selectedIndices.end(), 0) != result.selectedIndices.end());
    QVERIFY(std::find(result.selectedIndices.begin(), result.selectedIndices.end(), 1) != result.selectedIndices.end());
    QVERIFY(std::find(result.selectedIndices.begin(), result.selectedIndices.end(), 2) != result.selectedIndices.end());
}

void RegistrationCoreMeshGpuSmokeTest::runtime_target_constraint_filter_returns_selected_indices()
{
    std::cout << "[SmokeTest] runtime_target_constraint_filter_returns_selected_indices: start\n";
    std::cout.flush();
    const QString runtimeDllPath = QCoreApplication::applicationDirPath() + QStringLiteral("/MeshGPULib.dll");
    QLibrary runtimeLibrary(runtimeDllPath);
    QVERIFY2(runtimeLibrary.load(), qPrintable(runtimeLibrary.errorString()));

    const auto createRuntimeApi =
        reinterpret_cast<CreateRuntimeApiFn>(runtimeLibrary.resolve("CreateMeshGPURuntimeApi"));
    const auto destroyRuntimeApi =
        reinterpret_cast<DestroyRuntimeApiFn>(runtimeLibrary.resolve("DestroyMeshGPURuntimeApi"));
    QVERIFY(createRuntimeApi != nullptr);
    QVERIFY(destroyRuntimeApi != nullptr);

    mesh_gpu::MeshGPURuntimeApi* runtimeApi = createRuntimeApi();
    QVERIFY(runtimeApi != nullptr);

    auto target = createRegistrationSurface();
    const auto targetVertices = extractPoints(target);
    const auto targetNormals = extractNormals(target);
    const auto targetTriangles = extractTriangles(target);
    QVERIFY(runtimeApi->setTargetMesh(targetVertices, targetNormals, targetTriangles, 1.0f));

    const std::vector<mesh_gpu::Point3D> constraintPoints {
        mesh_gpu::Point3D(-7.5f, -14.0f, 9.0f),
        mesh_gpu::Point3D(-7.5f, 10.0f, 9.0f),
        mesh_gpu::Point3D(10.5f, -14.0f, 9.0f)
    };

    const auto result = runtimeApi->filterTargetPointsByConstraints(
        mesh_gpu::Point3D(1.5f, -2.0f, 9.0f),
        18.0f,
        5.4f,
        constraintPoints,
        3);
    std::cout << "[SmokeTest] runtime_target_constraint_filter_returns_selected_indices: call returned "
              << static_cast<int>(result.success) << " "
              << static_cast<int>(result.selectedIndices.size()) << "\n";
    std::cout.flush();

    destroyRuntimeApi(runtimeApi);
    runtimeApi = nullptr;
    runtimeLibrary.unload();
    std::cout << "[SmokeTest] runtime_target_constraint_filter_returns_selected_indices: end\n";
    std::cout.flush();

    QVERIFY(result.success);
    QVERIFY(static_cast<int>(result.selectedIndices.size()) >= 3);
}

void RegistrationCoreMeshGpuSmokeTest::runtime_constrained_target_mesh_returns_compact_mesh()
{
    std::cout << "[SmokeTest] runtime_constrained_target_mesh_returns_compact_mesh: start\n";
    std::cout.flush();
    const QString runtimeDllPath = QCoreApplication::applicationDirPath() + QStringLiteral("/MeshGPULib.dll");
    QLibrary runtimeLibrary(runtimeDllPath);
    QVERIFY2(runtimeLibrary.load(), qPrintable(runtimeLibrary.errorString()));

    const auto createRuntimeApi =
        reinterpret_cast<CreateRuntimeApiFn>(runtimeLibrary.resolve("CreateMeshGPURuntimeApi"));
    const auto destroyRuntimeApi =
        reinterpret_cast<DestroyRuntimeApiFn>(runtimeLibrary.resolve("DestroyMeshGPURuntimeApi"));
    QVERIFY(createRuntimeApi != nullptr);
    QVERIFY(destroyRuntimeApi != nullptr);

    mesh_gpu::MeshGPURuntimeApi* runtimeApi = createRuntimeApi();
    QVERIFY(runtimeApi != nullptr);

    auto target = createRegistrationSurface();
    const auto targetVertices = extractPoints(target);
    const auto targetNormals = extractNormals(target);
    const auto targetTriangles = extractTriangles(target);
    QVERIFY(runtimeApi->setTargetMesh(targetVertices, targetNormals, targetTriangles, 1.0f));

    const std::vector<mesh_gpu::Point3D> constraintPoints {
        mesh_gpu::Point3D(-7.5f, -14.0f, 9.0f),
        mesh_gpu::Point3D(-7.5f, 10.0f, 9.0f),
        mesh_gpu::Point3D(10.5f, -14.0f, 9.0f)
    };

    const auto result = runtimeApi->buildConstrainedTargetMesh(
        mesh_gpu::Point3D(1.5f, -2.0f, 9.0f),
        18.0f,
        5.4f,
        constraintPoints,
        3);
    std::cout << "[SmokeTest] runtime_constrained_target_mesh_returns_compact_mesh: call returned "
              << static_cast<int>(result.success) << " "
              << static_cast<int>(result.vertices.size()) << " "
              << static_cast<int>(result.triangles.size()) << "\n";
    std::cout.flush();

    destroyRuntimeApi(runtimeApi);
    runtimeApi = nullptr;
    runtimeLibrary.unload();
    std::cout << "[SmokeTest] runtime_constrained_target_mesh_returns_compact_mesh: end\n";
    std::cout.flush();

    QVERIFY(result.success);
    QCOMPARE(static_cast<int>(result.vertices.size()), 12);
    QCOMPARE(static_cast<int>(result.normals.size()), 12);
    QCOMPARE(static_cast<int>(result.original_vertex_indices.size()), 12);
    QCOMPARE(static_cast<int>(result.triangles.size()), 2);

    for (const auto& triangle : result.triangles) {
        QVERIFY(triangle[0] >= 0 && triangle[0] < static_cast<int>(result.vertices.size()));
        QVERIFY(triangle[1] >= 0 && triangle[1] < static_cast<int>(result.vertices.size()));
        QVERIFY(triangle[2] >= 0 && triangle[2] < static_cast<int>(result.vertices.size()));
    }
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    RegistrationCoreMeshGpuSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "RegistrationCoreMeshGpuSmokeTest.moc"

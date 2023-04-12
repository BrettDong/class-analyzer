#include <clang/AST/AST.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

static llvm::cl::OptionCategory ClassAnalyzerCategory("Class Analyzer Options");
[[maybe_unused]] static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

class ExtractFieldFrontendAction : public ASTFrontendAction {
    public:
        explicit ExtractFieldFrontendAction(std::string ClassName)
            : mClassName(std::move(ClassName)) {}

        auto CreateASTConsumer(CompilerInstance &CI, StringRef InFile)
            -> std::unique_ptr<ASTConsumer> override {
            auto Matcher =
                fieldDecl(hasParent(cxxRecordDecl(isClass(), hasName(mClassName)))).bind("field");
            mMatchFinder.addMatcher(Matcher, &mCallback);
            return mMatchFinder.newASTConsumer();
        }

    protected:
        class Callback : public MatchFinder::MatchCallback {
            public:
                void run(const MatchFinder::MatchResult &Result) override {
                    //
                }
        };

        std::string mClassName;
        MatchFinder mMatchFinder;
        Callback mCallback;
};

class FieldUsageFrontendAction : public ASTFrontendAction {
    public:
        explicit FieldUsageFrontendAction(std::string ClassName)
            : mClassName(std::move(ClassName)) {}

        auto CreateASTConsumer(CompilerInstance &CI, StringRef InFile)
            -> std::unique_ptr<ASTConsumer> override {
            auto Matcher =
                memberExpr(
                    hasObjectExpression(hasType(pointsTo(cxxRecordDecl(hasName(mClassName))))),
                    hasAncestor(cxxMethodDecl(ofClass(hasName(mClassName))).bind("method")))
                    .bind("member");
            mMatchFinder.addMatcher(Matcher, &mCallback);
            return mMatchFinder.newASTConsumer();
        }

    private:
        class Callback : public MatchFinder::MatchCallback {
            public:
                void run(const MatchFinder::MatchResult &Result) override {
                    const auto *Method = Result.Nodes.getNodeAs<CXXMethodDecl>("method");
                    const auto *Member = Result.Nodes.getNodeAs<MemberExpr>("member");
                    if (Method == nullptr) {
                        llvm::errs() << "Method is nullptr\n";
                        return;
                    }
                    if (Member == nullptr) {
                        llvm::errs() << "Member is nullptr\n";
                        return;
                    }
                    if (Member->getMemberDecl()->isFunctionOrFunctionTemplate()) {
                        return;
                    }
                    auto &DiagEng = Result.Context->getDiagnostics();
                    auto DiagID = DiagEng.getCustomDiagID(DiagnosticsEngine::Remark,
                                                          "Usage of field %0 in method %1");
                    auto DiagBuilder = DiagEng.Report(Member->getMemberLoc(), DiagID);
                    DiagBuilder.AddString(Member->getMemberDecl()->getName());
                    DiagBuilder.AddString(Method->getName());
                    DiagBuilder.AddSourceRange(
                        CharSourceRange::getCharRange(Member->getMemberLoc()));
                }
        };

        std::string mClassName;
        MatchFinder mMatchFinder;
        Callback mCallback;
};

class FieldUsageFrontendActionFactory : public FrontendActionFactory {
    public:
        std::unique_ptr<FrontendAction> create() override {
            return std::make_unique<FieldUsageFrontendAction>("Character");
        }
};

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, ClassAnalyzerCategory);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
    auto FrontendActionFactory = std::make_unique<FieldUsageFrontendActionFactory>();
    return Tool.run(FrontendActionFactory.get());
}
